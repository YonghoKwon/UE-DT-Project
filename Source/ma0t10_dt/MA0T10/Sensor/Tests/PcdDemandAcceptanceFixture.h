#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "EngineUtils.h"

inline bool IsPcdDemandAcceptanceFixture()
{ return FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD_DEMAND_ACCEPTANCE"))==TEXT("1"); }

/** Same native devices, poses, preview and full compatibility settings in both tests. */
inline bool CreatePcdDemandAcceptanceDevices(UWorld* World, AVirtualSensorCoordinator* Coordinator,
    const FTransform& LidarPose, const FTransform& CameraPose, int32 Pairs,
    TArray<AVirtualSensorActorBase*>& Devices, FString& Error)
{
    auto* Layout=World->GetSubsystem<UVirtualSensorLayoutSubsystem>();
    if(!Layout||!Coordinator){Error=TEXT("PCD acceptance services unavailable");return false;}
    AVirtualSensorExternalSourceHostActor* Receiver=nullptr;
    for(TActorIterator<AVirtualSensorExternalSourceHostActor> It(World);It;++It){Receiver=*It;break;}
    if(!Receiver)Receiver=World->SpawnActor<AVirtualSensorExternalSourceHostActor>();
    if(!Receiver){Error=TEXT("PCD acceptance UI receiver unavailable");return false;}
    const auto& Profile=Coordinator->SharedTransportComponent->GetTransportProfile();
    Receiver->ConfigureReceiverTopics(Profile.LidarTopic,Profile.CameraTopic,Profile.ExportTopic);Receiver->StartTopicReceivers();
    const auto Existing=Coordinator->GetSensorActors();
    for(auto* Sensor:Existing)if(IsValid(Sensor)){Sensor->StopSensor();Coordinator->UnregisterSensorActor(Sensor);}
    for(int32 Index=0;Index<Pairs;++Index)
    {
        auto LP=LidarPose,CP=CameraPose;
        LP.AddToTranslation(FVector(0,Index*50,0));CP.AddToTranslation(FVector(0,Index*50,0));
        auto* L=Cast<AVirtualLidarSensorActor>(Layout->CreateSensorAtPose(EVirtualSensorKind::Lidar,LP,Error,false));
        auto* C=Cast<AVirtualCameraSensorActor>(Layout->CreateSensorAtPose(EVirtualSensorKind::Camera,CP,Error,false));
        if(!L||!C)return false;
        FVirtualSensorEditableState State;L->ReadEditableState(State);
        const bool Bounded=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD_JSON_POLICY"))==TEXT("legacy-bounded");
        State.ServerPayloadStride=Bounded?32:1;State.MaxServerPayloadPoints=Bounded?1024:0;
        State.CompatibilityOutputPolicy=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD_JSON_POLICY"))==TEXT("requested")
            ?EVirtualLidarCompatibilityOutputPolicy::WhenRequested:EVirtualLidarCompatibilityOutputPolicy::LegacyAlways;
        if(!L->ApplyEditableState(State,Error))return false;
        L->ScanComponent->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
        L->VisualizationComponent->SetWorldPointCloudEnabled(false);
        L->StartSensor();C->StartSensor();Devices.Add(L);Devices.Add(C);
    }
    return Devices.Num()==Pairs*2;
}
#endif
