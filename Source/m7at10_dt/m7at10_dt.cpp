// Copyright Epic Games, Inc. All Rights Reserved.

#include "m7at10_dt.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, m7at10_dt, "m7at10_dt" );

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogM7AT10);
DEFINE_LOG_CATEGORY(LogBase);

// [추가] LogDx 카테고리의 실제 구현부입니다.
// 헤더에서 #else (Shipping이 아닐 때) 부분에 선언했으므로, 여기서도 조건을 맞춰주거나
// Shipping이 아닐 때만 정의되도록 합니다.
#if !UE_BUILD_SHIPPING
	DEFINE_LOG_CATEGORY(LogDx);
#endif