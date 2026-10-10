#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorOutputPreflightTest,"MA0T10.SensorLayout.OutputPreflight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorOutputPreflightTest::RunTest(const FString&)
{
    FVirtualSensorOutputPreflightResult S;
    for(int32 I=0;I<2;++I){auto& R=S.Streams.AddDefaulted_GetRef();R.SensorId=FString::FromInt(I);R.RequestedHz=20;R.EstimatedBytesPerFrame=576*56*33+4096;}
    auto R=UVirtualSensorOutputPreflight::Evaluate(S);TestEqual(TEXT("PCD-only two lidar is 40 messages/s"),R.MessagesPerSecond,40.0);TestTrue(TEXT("unknown FPS labelled assumption"),R.bAssumedFps);TestEqual(TEXT("default budget remains two"),R.SubmissionsPerFrame,2);
    for(int32 I=0;I<2;++I){auto& Camera=S.Streams.AddDefaulted_GetRef();Camera.SensorId=FString::FromInt(I);Camera.Kind=EVirtualSensorStreamKind::CameraImage;Camera.RequestedHz=30;auto& Lidar=S.Streams.AddDefaulted_GetRef();Lidar.SensorId=FString::FromInt(I);Lidar.Kind=EVirtualSensorStreamKind::LidarPayload;Lidar.RequestedHz=20;}
    const auto Duplicate=S.Streams[0];S.Streams.Add(Duplicate);R=UVirtualSensorOutputPreflight::Evaluate(S);TestEqual(TEXT("deduplicated three stream demand"),R.MessagesPerSecond,140.0);TestTrue(TEXT("120 submissions/s warns at 140 demand"),!R.Warnings.IsEmpty());
    S.SubmissionsPerFrame=4;R=UVirtualSensorOutputPreflight::Evaluate(S);TestEqual(TEXT("explicit four budget calculated, never auto changed"),R.TheoreticalSubmissionHz,240.0);
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* C=W->SpawnActor<AVirtualSensorCoordinator>();auto* L=W->GetSubsystem<UVirtualSensorLayoutSubsystem>();FString Error;
    auto* A=L->CreateSensorAtPose(EVirtualSensorKind::Lidar,FTransform(FVector(1,2,3)),Error,false);auto* Camera=L->CreateSensorAtPose(EVirtualSensorKind::Camera,FTransform::Identity,Error,false);
    if(!A||!Camera){AddError(Error);return false;}
    A->BeginInteractiveManipulation(FVirtualSensorInteractionRequest());const auto Before=A->GetActorTransform();const int32 Revision=A->GetConfigurationRevision();FVirtualSlabSensorOutputSelection O;
    for(int32 I=0;I<10;++I)R=UVirtualSensorOutputPreflight::Inspect(W,O,{});
    TestEqual(TEXT("preview camera not counted as JPEG send"),R.Streams.Num(),1);TestTrue(TEXT("original native dimensions used during manipulation"),R.Streams[0].EstimatedBytesPerFrame>=576*56*33);
    TestTrue(TEXT("repeated read leaves actor/config/manipulation intact"),A->GetActorTransform().Equals(Before)&&Revision==A->GetConfigurationRevision()&&A->IsInteractiveManipulationActive());
    TestTrue(TEXT("stopped camera stays stopped"),!Camera->IsSensorRunning());TestEqual(TEXT("submission budget unchanged"),C->StreamPublisherComponent->MaxSubmissionsPerFrame,2);
    for(const auto& Status:C->StreamPublisherComponent->GetStreamStatuses())TestFalse(TEXT("inspection starts no Topic"),Status.bEnabled);
    A->EndInteractiveManipulation();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorOutputPreflightRoutesTest,"MA0T10.SensorLayout.PreflightEffectiveRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorOutputPreflightRoutesTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* C=W->SpawnActor<AVirtualSensorCoordinator>();auto* L=W->GetSubsystem<UVirtualSensorLayoutSubsystem>();FString Error;
    auto* A=L->CreateSensorAtPose(EVirtualSensorKind::Lidar,FTransform::Identity,Error,false);
    auto* Camera=L->CreateSensorAtPose(EVirtualSensorKind::Camera,FTransform::Identity,Error,false);
    if(!A||!Camera){AddError(Error);return false;}
    auto* P=C->StreamPublisherComponent.Get();auto* T=C->SharedTransportComponent.Get();T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
    auto Profile=T->GetTransportProfile();Profile.BrokerUrl=TEXT("ws://127.0.0.1:61616");T->ConfigureTransportProfile(Profile);
    FVirtualSensorStreamConfig Config;Config.bEnabled=true;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;P->ConfigureStream(Config);
    FVirtualSensorStreamConfig Exact=Config;Exact.SensorId=A->GetSensorId();Exact.bEnabled=false;P->ConfigureStream(Exact);
    FVirtualSensorStreamConfig Route;
    TestTrue(TEXT("disabled exact does not shadow active wildcard"),P->ResolveActiveStreamForSensor(Config.StreamKind,A->GetSensorId(),Route)&&Route.SensorId.IsEmpty());
    Config.StreamKind=EVirtualSensorStreamKind::LidarPayload;Config.FrameStride=2;Config.TransportBackend=EVirtualSensorStreamTransportBackend::EngineStompCompatibility;P->ConfigureStream(Config);
    Exact=Config;Exact.SensorId=A->GetSensorId();Exact.FrameStride=4;P->ConfigureStream(Exact);
    TestTrue(TEXT("enabled exact shadows wildcard with its stride/backend"),P->ResolveActiveStreamForSensor(Config.StreamKind,A->GetSensorId(),Route)&&Route.SensorId==A->GetSensorId()&&Route.FrameStride==4);
    Config.StreamKind=EVirtualSensorStreamKind::CameraImage;Config.FrameStride=3;P->ConfigureStream(Config);
    FVirtualSlabSensorOutputSelection O;
    auto R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{A->GetSensorId()},EVirtualSensorPreflightPurpose::ScenarioTransition,60);
    TestEqual(TEXT("scenario replaces controlled telemetry but keeps independent camera"),R.Streams.Num(),2);
    TestEqual(TEXT("PCD is distinguished from total output demand"),R.PcdMessagesPerSecond,20.0);
    TestTrue(TEXT("non-target wildcard camera contributes 10Hz"),FMath::IsNearlyEqual(R.MessagesPerSecond,30.0,.001));
    TestTrue(TEXT("mixed outputs require both backend connections"),R.bRequiresRaw&&R.bRequiresEngine&&!R.bTransportConnected);
    R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{},EVirtualSensorPreflightPurpose::ScenarioTransition,60);
    TestEqual(TEXT("PCD-only all-target scenario excludes camera and telemetry"),R.Streams.Num(),1);
    TestTrue(TEXT("only requested raw connection is required"),R.bRequiresRaw&&!R.bRequiresEngine);
    Exact=P->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,A->GetSensorId());Exact.bEnabled=true;Exact.TransportBackend=EVirtualSensorStreamTransportBackend::EngineStompCompatibility;P->ConfigureStream(Exact);
    R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{},EVirtualSensorPreflightPurpose::ScenarioTransition,60);
    TestTrue(TEXT("explicit engine stream readiness never uses raw connection"),!R.bRequiresRaw&&R.bRequiresEngine);
    R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{A->GetSensorId()},EVirtualSensorPreflightPurpose::ManualStreamStart,60);
    TestEqual(TEXT("manual addition retains unrelated existing outputs exactly once"),R.Streams.Num(),3);
    TestTrue(TEXT("manual native PCD start uses its actual default raw backend"),R.bRequiresRaw&&R.bRequiresEngine);
    R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{},EVirtualSensorPreflightPurpose::ManualStreamStart,60);
    TestTrue(TEXT("manual empty target is not silently all sensors"),R.BlockingReasons.ContainsByPredicate([](const auto& S){return S.Contains(TEXT("선택 센서 없음"));}));
    auto* Scan=CastChecked<AVirtualLidarSensorActor>(A)->ScanComponent.Get();Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bUseProfileEchoCapability=false;Scan->bUseMultiHit=true;Scan->MaxHitsPerRay=2;
    R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{A->GetSensorId()},EVirtualSensorPreflightPurpose::ScenarioTransition,60);
    const auto* Pcd=R.Streams.FindByPredicate([](const auto& S){return S.Kind==EVirtualSensorStreamKind::PointCloud;});
    TestTrue(TEXT("GPU fallback keeps CPU echo upper bound and labels uncertainty"),Pcd&&Pcd->bEstimateUncertain&&Pcd->EstimatedBytesPerFrame>=int64(576)*56*2*33);
    Profile.ExportTopic.Reset();T->ConfigureTransportProfile(Profile);
    R=UVirtualSensorOutputPreflight::InspectForPurpose(W,O,{A->GetSensorId()},EVirtualSensorPreflightPurpose::ScenarioTransition,60);
    TestTrue(TEXT("missing PCD topic is a blocking reason"),R.BlockingReasons.ContainsByPredicate([](const auto& S){return S.Contains(TEXT("PCD Topic"));}));
    FVirtualSensorOutputPreflightResult Large;Large.PcdBudgetMegabytesPerSecond=1;auto& Row=Large.Streams.AddDefaulted_GetRef();Row.SensorId=TEXT("large-slow");Row.RequestedHz=.1;Row.EstimatedBytesPerFrame=2*1024*1024;
    R=UVirtualSensorOutputPreflight::Evaluate(Large);
    TestTrue(TEXT("single frame bucket warning even below average bandwidth"),R.PcdMegabytesPerSecond<1&&R.Warnings.ContainsByPredicate([](const auto& S){return S.Contains(TEXT("bucket"));}));
    P->StopAllStreams(FString());P->StopAllStreams(A->GetSensorId());return true;
}
#endif
