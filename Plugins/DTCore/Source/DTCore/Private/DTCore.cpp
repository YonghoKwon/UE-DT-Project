// Copyright Epic Games, Inc. All Rights Reserved.

#include "DTCore.h"

#define LOCTEXT_NAMESPACE "FDTCoreModule"

void FDTCoreModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FDTCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDTCoreModule, DTCore)

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogBase);

// [추가] LogDx 카테고리의 실제 구현부입니다.
// 헤더에서 #else (Shipping이 아닐 때) 부분에 선언했으므로, 여기서도 조건을 맞춰주거나
// Shipping이 아닐 때만 정의되도록 합니다.
#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY(LogDx);
#endif