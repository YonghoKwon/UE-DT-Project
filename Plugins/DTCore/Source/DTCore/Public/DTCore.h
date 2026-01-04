// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBase, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogDx, Log, All);

// 매크로 정의
#if UE_BUILD_SHIPPING
// Shipping: DxGameInstance의 정적 함수(GetInstance)를 통해 호출
#define DX_LOG(Format, ...) \
if (UDxGameInstance* GInst = UDxGameInstance::GetInstance()) \
{ \
GInst->WriteCustomLog(FString::Printf(TEXT(Format), ##__VA_ARGS__)); \
}
#else
// Development: 그냥 일반 로그 (파일로도 남기고 싶으면 여기에 WriteCustomLog 추가 가능)
#define DX_LOG(Format, ...) UE_LOG(LogDx, Log, TEXT(Format), ##__VA_ARGS__)
#endif

class FDTCoreModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
