#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"

namespace {
struct FDemandProof {int32 Acquired=0,Derived=0,Manual=0,Cancelled=0;int64 ManualFrame=-1;bool Coherent=false;FString ExportPath;};
class FWaitDemandOutput:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;TWeakObjectPtr<UVirtualLidarScanComponent> Sensor;TSharedRef<FDemandProof> Proof;
    TFunction<void()> Advance;double Started=FPlatformTime::Seconds();int32 Stage=0;
public:
    FWaitDemandOutput(FAutomationTestBase* T,UVirtualLidarScanComponent* C,TSharedRef<FDemandProof> P,TFunction<void()> A):Test(T),Sensor(C),Proof(P),Advance(MoveTemp(A)){}
    bool Update() override
    {
        if(!Sensor.IsValid()||FPlatformTime::Seconds()-Started>5){Test->AddError(TEXT("JSON demand fixture timed out"));return true;}
        auto* C=Sensor.Get();
        if(Stage==0)
        {
            if(Proof->Derived<1)return false;
            Test->TestEqual(TEXT("accepted derivation completes after consumer release"),Proof->Derived,1);
            Test->TestFalse(TEXT("released consumers no longer demand JSON"),C->GetCompatibilityOutputStatus().bRequested);
            FString Pcd;Test->TestTrue(TEXT("automatic PCD export survives omitted JSON"),FFileHelper::LoadFileToString(Pcd,*Proof->ExportPath)&&Pcd.Contains(TEXT("DATA ascii"))&&Pcd.Contains(TEXT("POINTS 1")));
            FString Id,Reason;C->JsonEncodeDelayMsForTests=100;
            Test->TestTrue(TEXT("manual immutable request accepted"),C->RequestCompatibilityPayloadFromLatestFrame(Id,Reason));
            Test->TestEqual(TEXT("manual request causes no acquisition"),Proof->Acquired,11);
            Advance();Stage=1;return false;
        }
        if(Stage==1)
        {
            if(Proof->Manual<1)return false;
            Test->TestEqual(TEXT("manual output retains requested older frame"),Proof->ManualFrame,int64(11));
            Test->TestTrue(TEXT("manual body and immutable metadata agree"),Proof->Coherent);
            Test->TestEqual(TEXT("manual output does not route automatic derived output"),Proof->Derived,1);
            FString Id,Reason;Test->TestTrue(TEXT("second manual request accepted"),C->RequestCompatibilityPayloadFromLatestFrame(Id,Reason));
            Test->TestFalse(TEXT("duplicate manual request rejects busy"),C->RequestCompatibilityPayloadFromLatestFrame(Id,Reason));
            C->StopScan();Started=FPlatformTime::Seconds();Stage=2;return false;
        }
        if(FPlatformTime::Seconds()-Started<.3)return false;
        Test->TestEqual(TEXT("stop reports one cancellation"),Proof->Cancelled,1);
        Test->TestEqual(TEXT("late cancelled completion cannot apply"),Proof->Manual,1);
        Test->TestEqual(TEXT("global JSON jobs returned"),C->GetResourceSnapshot().Counts.FindRef(TEXT("global_json_jobs")),int64(0));
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarDemandPolicyTest,"MA0T10.LidarOutput.DemandPolicy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarDemandPolicyTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* A=W->SpawnActor<AVirtualLidarSensorActor>();auto* C=A->ScanComponent.Get();C->StopScan();C->OutputMode=EVirtualLidarOutputMode::None;
    TestEqual(TEXT("existing classes remain legacy"),C->CompatibilityOutputPolicy,EVirtualLidarCompatibilityOutputPolicy::LegacyAlways);
    FVirtualSensorEditableState State;A->ReadEditableState(State);const auto Revision=UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(A);
    const auto Generation=C->GetFileCaptureRevision();State.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;FString Reason;
    TestTrue(TEXT("actor applies explicit output policy"),A->ApplyEditableState(State,Reason));
    TestEqual(TEXT("policy-only apply leaves acquisition generation"),C->GetFileCaptureRevision(),Generation);
    TestEqual(TEXT("policy-only apply preserves pending file revision"),UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(A),Revision);
    auto Json=FJsonObjectConverter::UStructToJsonObject(State);Json->RemoveField(TEXT("compatibilityOutputPolicy"));FVirtualSensorEditableState Old;
    TestTrue(TEXT("old additive layout state loads"),FJsonObjectConverter::JsonObjectToUStruct(Json.ToSharedRef(),&Old));
    TestEqual(TEXT("missing old policy stays legacy"),Old.CompatibilityOutputPolicy,EVirtualLidarCompatibilityOutputPolicy::LegacyAlways);
    auto P=MakeShared<FDemandProof>();C->OnFrameAcquiredNative.AddLambda([P](int64){++P->Acquired;});
    C->OnDerivedFrameCompletedNative.AddLambda([P](const auto&){++P->Derived;});
    C->OnCompatibilityRequestCompletedNative.AddLambda([P](const FString&,bool Success,const FVirtualSensorFrameEnvelope& Frame,const FString&){if(Success){++P->Manual;P->ManualFrame=Frame.FrameId;TSharedPtr<FJsonObject> Body;P->Coherent=Frame.HasJsonPayload()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(*Frame.JsonPayload),Body)&&int64(Body->GetNumberField(TEXT("frameId")))==Frame.FrameId&&Body->GetStringField(TEXT("timestampUtc"))==Frame.TimestampUtc.ToIso8601()&&Frame.LidarFrameSnapshot->FrameId==Frame.FrameId;}else ++P->Cancelled;});
    TWeakObjectPtr<UVirtualLidarScanComponent> Weak(C);
    auto Measure=[Weak](){auto* S=Weak.Get();S->ScheduledScanWidth=1;S->ScheduledScanHeight=1;S->bScheduledGenerateHeatmap=false;S->ActiveAcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
        auto& Point=S->ScheduledPoints.AddDefaulted_GetRef();Point.bHit=true;Point.Distance=100;Point.WorldLocation=FVector(100,0,0);S->ScheduledScanStartTime=FPlatformTime::Seconds();S->ScheduledAcquisitionStartUnixNanoseconds=(FDateTime::UtcNow()-FDateTime(1970,1,1)).GetTicks()*100;S->CompleteScheduledScan(S->GetWorld()->GetTimeSeconds());};
    for(int32 I=0;I<10;++I){C->bExportPcdOnScan=I==9;Measure();}P->ExportPath=C->GetLastPointCloudExportPath();C->bExportPcdOnScan=false;
    TestEqual(TEXT("PCD/preview acquisition does not start JSON"),C->GetCompatibilityOutputStatus().Started,int64(0));
    TestEqual(TEXT("all unused JSON frames explicitly counted"),C->GetCompatibilityOutputStatus().SkippedUnrequested,int64(10));
    const FString Token=C->RegisterCompatibilityJsonConsumer(A);Measure();C->UnregisterCompatibilityJsonConsumer(Token);
    ADD_LATENT_AUTOMATION_COMMAND(FWaitDemandOutput(this,C,P,MoveTemp(Measure)));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarDemandRoutingTest,"MA0T10.LidarOutput.DemandRoutingAndDuplication",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarDemandRoutingTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* Coordinator=W->SpawnActor<AVirtualSensorCoordinator>();auto* Publisher=Coordinator->StreamPublisherComponent.Get();
    auto Profile=Coordinator->SharedTransportComponent->GetTransportProfile();Profile.BrokerUrl=TEXT("ws://127.0.0.1:61616");Coordinator->SharedTransportComponent->ConfigureTransportProfile(Profile);Coordinator->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
    FVirtualSensorStreamConfig Global;Global.StreamKind=EVirtualSensorStreamKind::LidarPayload;Global.bEnabled=true;Global.TransportBackend=EVirtualSensorStreamTransportBackend::EngineStompCompatibility;Publisher->ConfigureStream(Global);
    TestTrue(TEXT("wildcard compatibility needs JSON"),Publisher->NeedsCompatibilityLidarOutput(TEXT("one")));
    auto Exact=Global;Exact.SensorId=TEXT("one");Exact.TransportBackend=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;Publisher->ConfigureStream(Exact);
    TestFalse(TEXT("exact raw route hides wildcard compatibility"),Publisher->NeedsCompatibilityLidarOutput(TEXT("one")));
    Exact.bEnabled=false;Publisher->ConfigureStream(Exact);TestTrue(TEXT("disabled exact permits global route"),Publisher->NeedsCompatibilityLidarOutput(TEXT("one")));
    Publisher->StopAllStreams(FString());
    auto* PC=W->SpawnActor<APlayerController>();PC->PlayerCameraManager=W->SpawnActor<APlayerCameraManager>();PC->PlayerCameraManager->InitializeFor(PC);
    if(!W->GetFirstPlayerController())W->AddController(PC);
    auto* Layout=W->GetSubsystem<UVirtualSensorLayoutSubsystem>();FString Reason;
    auto* A=Layout->CreateSensorAtPose(EVirtualSensorKind::Lidar,FTransform(FVector(10,20,30)),Reason,false);
    auto* Copy=Layout->DuplicateSensor(A,Reason);if(!Copy)AddInfo(Reason);TestNotNull(TEXT("duplicate device succeeds"),Copy);
    if(Copy){FVirtualSensorEditableState S;Copy->ReadEditableState(S);TestEqual(TEXT("duplicate preserves demand policy"),S.CompatibilityOutputPolicy,EVirtualLidarCompatibilityOutputPolicy::WhenRequested);TestTrue(TEXT("duplicate has fresh id"),Copy->GetSensorId()!=A->GetSensorId());Copy->StopSensor();TestTrue(TEXT("owned duplicate deletes safely"),Layout->DeleteSensor(Copy,Reason));}
    return true;
}
#endif
