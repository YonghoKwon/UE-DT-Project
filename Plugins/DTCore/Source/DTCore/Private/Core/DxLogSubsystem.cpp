#include "Core/DxLogSubsystem.h"

#include "DTCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Async/Async.h" // 비동기 처리를 위해 필수

void UDxLogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 로그 테스트
	DX_LOG(GetWorld(), TEXT("========================================="));
	DX_LOG(GetWorld(), TEXT("   SHIPPING LOG STARTED - SYSTEM INIT    "));
	DX_LOG(GetWorld(), TEXT("========================================="));

	// 게임 시작 시 7일 지난 로그 삭제 (백그라운드에서 실행)
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
	{
		DeleteOldLogs(7);
	});
}

void UDxLogSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxLogSubsystem::WriteLog(const FString& LogContent, bool bPrintToScreen, FString LogFileName)
{
	FDateTime Now = FDateTime::Now();
	FString TimeStamp = Now.ToString(TEXT("[%Y-%m-%d %H:%M:%S.%s] "));
	FString FinalLog = TimeStamp + LogContent + TEXT("\n");

	// 1. 화면 출력 (GameThread)
	if (bPrintToScreen)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, LogContent);
		}
#endif
		UE_LOG(LogTemp, Log, TEXT("%s"), *LogContent);
	}

	// 2. 파일 이름 결정
	FString TargetFileName = LogFileName;
	if (TargetFileName.IsEmpty())
	{
		TargetFileName = FString::Printf(TEXT("DxLog_%s.log"), *Now.ToString(TEXT("%Y-%m-%d")));
	}

	// 3. [중요] 버퍼에 "순서대로" 추가 (Lock 사용)
	{
		// BufferMutex를 잠그고 안전하게 추가
		FScopeLock Lock(&BufferMutex);
		LogBuffer.Emplace(TargetFileName, FinalLog);
	}

	// 4. [중요] 현재 쓰는 놈(Consumer)이 없으면, 하나 깨워서 일 시킴
	// bIsWriting이 false였다면 true로 바꾸고 if문 진입 (Atomic)
	if (!bIsWriting.AtomicSet(true))
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
		{
			ProcessLogBuffer();
		});
	}
}

void UDxLogSubsystem::DeleteOldLogs(int32 RetentionDays)
{
	IFileManager& FileManager = IFileManager::Get();
	FString LogDir = GetLogDirectory();

	// 1. 로그 디렉토리 확인
	if (!FileManager.DirectoryExists(*LogDir))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DxLog] Log Directory not found: %s"), *LogDir);
		return;
	}

	// 2. 파일 목록 가져오기 (*.log)
	TArray<FString> FoundFiles;
	FileManager.FindFiles(FoundFiles, *(LogDir / TEXT("*.log")), true, false);

	DX_LOG(GetWorld(), TEXT("[DxLog] Checking %d files for cleanup... (Retention: %d Days)"), FoundFiles.Num(), RetentionDays);

	FDateTime Now = FDateTime::Now();
	FTimespan RetentionSpan = FTimespan::FromDays(RetentionDays);

	for (const FString& FileName : FoundFiles)
	{
		// FileName 예시: "DxLog_2024-01-04.log"
		FString FullPath = LogDir / FileName;
		FDateTime FileDate;
		bool bValidDate = false;

		// [중요] 파일 시스템 시간이 아니라, '파일 이름'에서 날짜를 분석합니다.
		// 1. 확장자 제거 -> "DxLog_2024-01-04"
		FString BaseName = FPaths::GetBaseFilename(FileName);

		// 2. 접두사("DxLog_") 제거 -> "2024-01-04"
		if (BaseName.StartsWith(TEXT("DxLog_")))
		{
			FString DateString = BaseName.RightChop(6); // 앞의 6글자(DxLog_) 자름

			// 3. 문자열을 날짜로 변환 (ISO 8601 포맷: YYYY-MM-DD)
			if (FDateTime::ParseIso8601(*DateString, FileDate))
			{
				bValidDate = true;
			}
		}

		// 파일 이름 분석에 실패했다면, 기존대로 파일 수정 시간을 사용 (백업 로직)
		if (!bValidDate)
		{
			FileDate = FileManager.GetTimeStamp(*FullPath);
			UE_LOG(LogTemp, Warning, TEXT("[DxLog] Name parsing failed for '%s'. Using File System Timestamp."), *FileName);
		}

		// 날짜 차이 계산
		FTimespan Age = Now - FileDate;

		if (Age > RetentionSpan)
		{
			if (FileManager.Delete(*FullPath))
			{
				UE_LOG(LogTemp, Log, TEXT("[DxLog] [DELETED] %s (Log Date: %s, Age: %.1f Days)"),
					*FileName, *FileDate.ToString(TEXT("%Y-%m-%d")), Age.GetTotalDays());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[DxLog] [FAIL] Could not delete %s"), *FileName);
			}
		}
		else
		{
			// 너무 최신 파일임
			// UE_LOG(LogTemp, Log, TEXT("[DxLog] [KEEP] %s (Age: %.1f Days)"), *FileName, Age.GetTotalDays());
		}
	}
}

FString UDxLogSubsystem::GetLogDirectory()
{
	// 프로젝트 저장 폴더 아래 Logs/CustomLogs
	return FPaths::LaunchDir() / TEXT("Logs") / TEXT("CustomLogs");
	// return FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("CustomLogs");
}

void UDxLogSubsystem::ProcessLogBuffer()
{
	// 이 함수는 백그라운드 스레드에서 실행됩니다.
	// 한 번 실행되면 버퍼가 빌 때까지 계속 반복합니다.

	while (true)
	{
		TArray<TPair<FString, FString>> LocalBuffer;

		// 1. 메인 버퍼의 내용을 몽땅 가져옴 (Lock 시간을 최소화하기 위해)
		{
			FScopeLock Lock(&BufferMutex);
			if (LogBuffer.Num() == 0)
			{
				// 더 이상 처리할 게 없으면 작업 종료 표시 후 리턴
				bIsWriting = false;
				return;
			}

			// MoveTemp로 복사 비용 없이 소유권 이전
			LocalBuffer = MoveTemp(LogBuffer);
			// LogBuffer는 이제 비어있음
		}

		// 2. 파일에 쓰기 (Lock 없이 수행 -> 다른 스레드는 이 동안에도 WriteLog 가능)
		IFileManager& FileManager = IFileManager::Get();
		FString LogDir = GetLogDirectory();

		// 파일별로 내용을 묶어서 저장 (최적화)
		// 같은 파일에 쓸 내용을 합쳐서 한 번에 I/O 하는 것이 좋음
		for (const auto& LogEntry : LocalBuffer)
		{
			FString FullPath = LogDir / LogEntry.Key;

			if (!FileManager.DirectoryExists(*LogDir))
			{
				FileManager.MakeDirectory(*LogDir, true);
			}

			FFileHelper::SaveStringToFile(LogEntry.Value, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8,
			                              &FileManager, FILEWRITE_Append);
		}

		// 루프 다시 시작 -> 그 사이 쌓인 로그가 있는지 확인하러 감
	}
}
