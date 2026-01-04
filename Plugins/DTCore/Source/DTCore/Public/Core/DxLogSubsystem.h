#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxLogSubsystem.generated.h"

UCLASS()
class DTCORE_API UDxLogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// Function
public:
	// 서브시스템 초기화 (여기서 오래된 로그를 정리합니다)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/**
	 * 로그를 파일에 씁니다. (비동기 처리되어 게임 멈춤 없음)
	 * @param LogContent : 남길 로그 내용
	 * @param bPrintToScreen : 화면에도 출력할지 여부
	 * @param LogFileName : 저장할 파일 이름 (기본값: 날짜별 자동 생성)
	 */
	void WriteLog(const FString& LogContent, bool bPrintToScreen = false, FString LogFileName = TEXT(""));
private:
	void DeleteOldLogs(int32 RetentionDays = 7);
	FString GetLogDirectory();
	// [NEW] 로그 파일에 실제로 쓰는 함수 (백그라운드에서 실행됨)
	void ProcessLogBuffer();
protected:

	// Variable
public:
private:
	// [NEW] 로그를 잠시 담아둘 버퍼
	TArray<TPair<FString, FString>> LogBuffer; // <FileName, Content>
	// [NEW] 버퍼 접근 시 충돌 방지용 뮤텍스
	FCriticalSection BufferMutex;
	// [NEW] 현재 파일 쓰기 작업이 진행 중인지 체크
	FThreadSafeBool bIsWriting;
protected:
};
