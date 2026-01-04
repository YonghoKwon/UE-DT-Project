#include "Core/DxGameInstance.h"

#include "DTCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h" // 디렉토리 생성을 위해 필요
#include "GameFramework/GameUserSettings.h"

UDxGameInstance* UDxGameInstance::Instance = nullptr;

void UDxGameInstance::Init()
{
	Super::Init();

	Instance = this;

	// 2일 지난 로그 파일 삭제
	DeleteOldLogs(2);

	// 로그 테스트
	DX_LOG("=========================================");
	DX_LOG("   SHIPPING LOG STARTED - SYSTEM INIT    ");
	DX_LOG("=========================================");

	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (UserSettings)
	{
		// Frame 60 고정
		UserSettings->SetFrameRateLimit(60.0f);
		UserSettings->ApplySettings(false);
	}
}

void UDxGameInstance::Shutdown()
{
	Instance = nullptr;
	Super::Shutdown();
}

UDxGameInstance* UDxGameInstance::GetInstance()
{
	return Instance;
}

void UDxGameInstance::WriteCustomLog(FString LogText)
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [LogText]()
	{
		// 경로 설정: 실행 파일(.exe)이 있는 폴더 기준
		// 결과 경로: [패키징폴더]/m7at10_dt/Binaries/Win64/Logs/
		FString BaseDir = FPaths::LaunchDir();
		FString LogFolder = FPaths::Combine(BaseDir, TEXT("Logs"));

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*LogFolder))
		{
			PlatformFile.CreateDirectoryTree(*LogFolder);
		}

		// 파일명 및 전체 경로 생성
		FString DateStr = FDateTime::Now().ToString(TEXT("%Y%m%d")); // 예: 20240101
		FString FileName = FString::Printf(TEXT("DxLog_%s.txt"), *DateStr);
		FString FullPath = FPaths::Combine(LogFolder, FileName);

		// 로그 내용 포맷팅
		FString TimeStr = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
		FString FinalLog = FString::Printf(TEXT("[%s] %s\r\n"), *TimeStr, *LogText);

		// 파일 쓰기
		FFileHelper::SaveStringToFile(FinalLog, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_Append);
	});
}

void UDxGameInstance::DeleteOldLogs(int32 RetentionDays)
{
	// 로그 폴더 경로 가져오기
	FString BaseDir = FPaths::LaunchDir();
	FString LogFolder = FPaths::Combine(BaseDir, TEXT("Logs"));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LogFolder))
	{
		return;
	}

	// 폴더 내의 모든 파일 찾기 (*.txt)
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *LogFolder, TEXT(".txt"));

	FDateTime CurrentTime = FDateTime::Now();

	// 파일 순회하며 날짜 검사
	for (const FString& FileName : FoundFiles)
	{
		// 전체 경로 생성
		FString FullPath = FPaths::Combine(LogFolder, FileName);
		FDateTime FileTimeStamp = IFileManager::Get().GetTimeStamp(*FullPath);
		FTimespan Diff = CurrentTime - FileTimeStamp;

		// 지정된 기간(일)보다 오래되었으면 삭제
		if (Diff.GetTotalDays() > RetentionDays)
		{
			// 로그를 남기고 삭제
			FString LogMsg = FString::Printf(TEXT("Deleting old log file: %s (Age: %.1f days)"), *FileName, Diff.GetTotalDays());
			WriteCustomLog(LogMsg);

			IFileManager::Get().Delete(*FullPath);
		}
	}
}
