#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
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
#endif
