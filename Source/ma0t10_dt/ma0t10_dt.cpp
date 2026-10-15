// Copyright Epic Games, Inc. All Rights Reserved.

#include "ma0t10_dt.h"
#include "Modules/ModuleManager.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualSensorStreamReceiverRegistration.h"
#include "ma0t10_dt/MA0T10/UI/PcdTransportRuntimeProbe.h"

class FMa0t10DtGameModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		RegisterVirtualSensorStreamReceiverCodec();
		StartPcdTransportRuntimeProbe();
	}
	virtual void ShutdownModule() override
	{
		StopPcdTransportRuntimeProbe();
		UnregisterVirtualSensorStreamReceiverCodec();
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMa0t10DtGameModule, ma0t10_dt, "ma0t10_dt");

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogMA0T10);
