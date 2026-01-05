// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
// Subsystem 헤더 포함
#include "Core/DxLogSubsystem.h"
#include "Kismet/GameplayStatics.h"

class FDTCoreModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogBase, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDx, Log, All);

// ================================================================================================
// [DX LOG MACROS]
// 동작 방식:
// 1. Shipping 빌드: 화면 출력 OFF, 파일 저장 ON (로그 파일 생성)
// 2. 그 외 (Editor/Dev): 화면 출력 ON, 파일 저장 OFF (콘솔 로그만 사용)
// ================================================================================================

#if UE_BUILD_SHIPPING
// [Shipping 모드] -> 파일에 기록 (화면 출력은 끔)
#define DX_LOG(WorldContext, Format, ...) \
{ \
/* 1. UE_LOG도 남겨둠 (Shipping에서 -log 옵션으로 볼 수 있음) */ \
UE_LOG(LogBase, Log, Format, ##__VA_ARGS__); \
\
/* 2. Subsystem을 통해 파일로 비동기 저장 */ \
if (UWorld* World = WorldContext) \
{ \
if (UGameInstance* GI = World->GetGameInstance()) \
{ \
/* GetSubsystem은 매우 빠르므로 매번 호출해도 괜찮습니다 */ \
if (UDxLogSubsystem* LogSys = GI->GetSubsystem<UDxLogSubsystem>()) \
{ \
FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
LogSys->WriteLog(Msg, false); /* bPrintToScreen = false */ \
} \
} \
} \
}

// [Shipping 전용] 파일 저장 매크로 (위와 동일하게 동작)
#define DX_LOG_FILE(WorldContext, Format, ...) DX_LOG(WorldContext, Format, ##__VA_ARGS__)

#else
// [Development / Editor 모드] -> 화면과 Output Log에만 출력 (파일 생성 안 함)
#define DX_LOG(WorldContext, Format, ...) \
{ \
/* 1. UE_LOG 출력 */ \
UE_LOG(LogBase, Log, Format, ##__VA_ARGS__); \
\
/* 2. 화면 출력 (Print String 효과) */ \
FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
if (GEngine) \
{ \
GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, FString::Printf(TEXT("[DT] %s"), *Msg)); \
} \
}

// 파일 저장 전용 매크로를 에디터에서 불렀을 경우 -> 그냥 로그만 찍고 넘어감 (파일 생성 방지)
#define DX_LOG_FILE(WorldContext, Format, ...) UE_LOG(LogBase, Log, Format, ##__VA_ARGS__)

#endif
