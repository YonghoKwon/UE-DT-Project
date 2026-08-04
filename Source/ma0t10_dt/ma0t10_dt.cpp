// Copyright Epic Games, Inc. All Rights Reserved.

#include "ma0t10_dt.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

class FMa0t10DtModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// UE 5.3 services libwebsockets at 30 Hz by default. A 1 MiB STOMP
		// frame is emitted over many writable callbacks, which limits a single
		// connection to well below ML-X(80)'s 20 Hz requirement. Configure the
		// worker before WebSockets is loaded; this changes only the in-memory
		// engine config and does not write DefaultEngine.ini/Game.ini.
		if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")) && GConfig)
		{
			GConfig->SetDouble(
				TEXT("WebSockets.LibWebSockets"),
				TEXT("ThreadTargetFrameTimeInSeconds"),
				0.001,
				GEngineIni);
		}
		FDefaultGameModuleImpl::StartupModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMa0t10DtModule, ma0t10_dt, "ma0t10_dt");

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogMA0T10);
