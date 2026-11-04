#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"
#include "Device/Sensor/DeviceSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceCoordinatorFacadeContract,
                                 "MA0T10.DeviceBoundary.CoordinatorFacadeContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceCoordinatorFacadeContract::RunTest(const FString&)
{
	UClass* Class = AVirtualSensorCoordinator::StaticClass();
	TestEqual(TEXT("legacy reflected path"), Class->GetPathName(), FString(TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator")));
	for (const TCHAR* Name : {TEXT("OnViewModeChanged"), TEXT("bDiscoverOnBeginPlay"), TEXT("bStartSensorsOnBeginPlay"),
	                          TEXT("bUseSynchronizedCapture"), TEXT("SynchronizedInterval"), TEXT("StaleSensorSeconds"),
	                          TEXT("bPointCloudOnlyHideWorld"), TEXT("bPointCloudOnlyAutoSelectLidarView"), TEXT("PointCloudOnlyKeepActorTags"),
	                          TEXT("SharedTransportComp"), TEXT("SharedRecorderComp"), TEXT("StreamPublisherComp")})
	{
		FProperty* Property = FindFProperty<FProperty>(Class, Name);
		TestNotNull(Name, Property);
		if (Property)
			TestEqual(TEXT("public serialized owner stays on facade"), Property->GetOwnerStruct(), static_cast<UStruct*>(Class));
	}
	UFunction* Bind = Class->FindFunctionByName(TEXT("BindMonitorWidget"));
	TestNotNull(TEXT("legacy BP binding function"), Bind);
	FObjectProperty* Pin = Bind ? FindFProperty<FObjectProperty>(Bind, TEXT("MonitorWidget")) : nullptr;
	TestTrue(TEXT("legacy widget pin type preserved"), Pin && Pin->PropertyClass == UDxWidgetVirtualSensorMonitorPanel::StaticClass());
	TestEqual(TEXT("view enum RealWorld"), static_cast<uint8>(EVirtualSensorViewMode::RealWorld), uint8(0));
	TestEqual(TEXT("view enum PointCloudOnly"), static_cast<uint8>(EVirtualSensorViewMode::PointCloudOnly), uint8(3));
	TestEqual(TEXT("legacy summary path"), FVirtualSensorSummary::StaticStruct()->GetPathName(), FString(TEXT("/Script/DeviceRuntime.VirtualSensorSummary")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceCoordinatorWeakMonitorLifetime,
                                 "MA0T10.DeviceBoundary.CoordinatorWeakMonitorLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceCoordinatorWeakMonitorLifetime::RunTest(const FString&)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world"), World))
		return false;
	AVirtualSensorCoordinator* Facade = World->SpawnActor<AVirtualSensorCoordinator>();
	if (!TestNotNull(TEXT("coordinator facade"), Facade))
		return false;
	Facade->bDiscoverOnBeginPlay = false;
	Facade->bStartSensorsOnBeginPlay = false;
	ADeviceSensorCoordinator* Runtime = Facade;
	TWeakObjectPtr<UDxWidgetVirtualSensorMonitorPanel> Weak = NewObject<UDxWidgetVirtualSensorMonitorPanel>();
	Runtime->BindDeviceMonitorView(Weak.Get());
	TestEqual(TEXT("one runtime monitor owner"), Runtime->GetDeviceMonitorOwner(), static_cast<UObject*>(Weak.Get()));
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("runtime binding is weak"), Weak.IsValid());
	TestNull(TEXT("expired owner is safe"), Runtime->GetDeviceMonitorOwner());
	Runtime->SetViewMode(EVirtualSensorViewMode::Lidar);
	TestEqual(TEXT("facade observes base state"), Facade->GetViewMode(), EVirtualSensorViewMode::Lidar);
	UDxWidgetVirtualSensorMonitorPanel* Strong = NewObject<UDxWidgetVirtualSensorMonitorPanel>();
	Weak = Strong;
	Facade->BindMonitorWidget(Strong);
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("legacy facade preserves its GC contract"), Weak.IsValid());
	Facade->BindMonitorWidget(nullptr);
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("explicit unbind releases facade ownership"), Weak.IsValid());
	TestNull(TEXT("base unbound together with facade"), Runtime->GetDeviceMonitorOwner());
	Facade->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceCoordinatorEndPlayCleanup,
                                 "MA0T10.DeviceBoundary.CoordinatorEndPlayCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceCoordinatorEndPlayCleanup::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Editor world"), World))
		return false;
	AVirtualSensorCoordinator* Facade = World->SpawnActor<AVirtualSensorCoordinator>();
	if (!TestNotNull(TEXT("facade"), Facade))
		return false;
	Facade->bDiscoverOnBeginPlay = false;
	Facade->bStartSensorsOnBeginPlay = false;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	TestTrue(TEXT("real actor initialization before BeginPlay"), Facade->IsActorInitialized());
	Facade->DispatchBeginPlay();
	TestTrue(TEXT("real actor BeginPlay before EndPlay"), Facade->HasActorBegunPlay());
	Facade->BindMonitorWidget(NewObject<UDxWidgetVirtualSensorMonitorPanel>());
	Facade->RouteEndPlay(EEndPlayReason::EndPlayInEditor);
	TestNull(TEXT("PIE EndPlay unbinds monitor"), Facade->GetDeviceMonitorOwner());
	TestEqual(TEXT("EndPlay clears inventory"), Facade->GetSensorActors().Num(), 0);
	Facade->Destroy();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceCoordinatorViewStateIsolation,
                                 "MA0T10.DeviceBoundary.CoordinatorViewStateIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceCoordinatorViewStateIsolation::RunTest(const FString&)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world"), World))
		return false;
	auto* A = World->SpawnActor<AVirtualSensorCoordinator>();
	auto* B = World->SpawnActor<AVirtualSensorCoordinator>();
	auto* LidarA = World->SpawnActor<AVirtualLidarSensorActor>();
	auto* LidarB = World->SpawnActor<AVirtualLidarSensorActor>();
	A->bPointCloudOnlyHideWorld = false;
	B->bPointCloudOnlyHideWorld = false;
	A->RegisterLidar(LidarA->ScanComp);
	B->RegisterLidar(LidarB->ScanComp);
	A->SetPointCloudOnlyMode(true);
	B->SetPointCloudOnlyMode(true);
	TestEqual(TEXT("first base owns only its snapshot"), A->LidarPointCloudOnlyStates.Num(), 1);
	TestEqual(TEXT("second base owns only its snapshot"), B->LidarPointCloudOnlyStates.Num(), 1);
	A->SetPointCloudOnlyMode(false);
	TestEqual(TEXT("first restoration empties its state"), A->LidarPointCloudOnlyStates.Num(), 0);
	TestEqual(TEXT("first restoration cannot clear another base"), B->LidarPointCloudOnlyStates.Num(), 1);
	B->SetPointCloudOnlyMode(false);
	TestEqual(TEXT("second restoration empties its state"), B->LidarPointCloudOnlyStates.Num(), 0);
	A->Destroy();
	B->Destroy();
	LidarA->Destroy();
	LidarB->Destroy();
	return true;
}
#endif
