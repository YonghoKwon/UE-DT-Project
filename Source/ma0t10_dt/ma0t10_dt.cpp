// Copyright Epic Games, Inc. All Rights Reserved.

#include "ma0t10_dt.h"
#include "Modules/ModuleManager.h"
#include "MA0T10/Scenario/ProjectDeviceAnalysisPolicy.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualSensorStreamReceiverRegistration.h"
#include "ma0t10_dt/MA0T10/UI/PcdTransportRuntimeProbe.h"
#include "ma0t10_dt/MA0T10/UI/HatchMemoryRuntimeProbe.h"

class FMa0t10DtGameModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		RegisterProjectDeviceAnalysisPolicy();
		RegisterVirtualSensorStreamReceiverCodec();
		StartPcdTransportRuntimeProbe();
		StartHatchMemoryRuntimeProbe();
	}
	virtual void ShutdownModule() override
	{
		StopHatchMemoryRuntimeProbe();
		StopPcdTransportRuntimeProbe();
		UnregisterVirtualSensorStreamReceiverCodec();
		UnregisterProjectDeviceAnalysisPolicy();
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMa0t10DtGameModule, ma0t10_dt, "ma0t10_dt");

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogMA0T10);
