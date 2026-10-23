#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/LineBatchComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarPreviewIdentityTransformTest,
    "MA0T10.LidarFidelity.PreviewIdentityTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLidarPreviewIdentityTransformTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Actor=World->SpawnActor<AVirtualLidarSensorActor>();
    auto* Scan=Actor->ScanComponent.Get();auto* View=Actor->VisualizationComponent.Get();
    View->BindScanComponent(Scan);View->SetProjectionPreviewEnabled(false);
    FVirtualLidarPreviewPolicy Policy=Scan->GetPreviewPolicyState();
    Policy.PointStride=1;Policy.MaxPoints=8;Policy.bHitOnly=false;Scan->SetPreviewPolicyState(Policy);
    TArray<FVirtualLidarPoint> Points;
    for(int32 I=0;I<8;++I)
    {
        FVirtualLidarPoint P;P.bHit=true;P.Distance=100+I;P.WorldLocation=FVector(100,I,10);
        P.HitActorTags={TEXT("tag-a"),TEXT("tag-b")};P.SemanticLabel=TEXT("TestSurface");Points.Add(P);
    }
    Scan->InjectPointCloudFrame(Points,false);
    const auto Snapshot=Scan->GetLastFrameSnapshot();const int64 FrameId=Snapshot->FrameId;
    int32 Calls=0,Preflights=0;
    View->SetPointPreviewTransform([&](FVirtualLidarPoint&,double){++Calls;});
    View->SetColoredCpuPointPreview(true);View->SetWorldPointCloudEnabled(true);View->RefreshPointPreview();
    if(!TestNotNull(TEXT("CPU preview batch exists"),View->ColoredPointBatch.Get())){Actor->Destroy();return false;}
    const auto Baseline=View->ColoredPointBatch->BatchedPoints;
    TestEqual(TEXT("fixture displays all8points"),Baseline.Num(),8);
    Calls=0;
    View->SetPointPreviewTransformNeeded([&](double){++Preflights;return false;});View->RefreshPointPreview();
    TestEqual(TEXT("identity preflight runs once per refreshed frame"),Preflights,1);
    TestEqual(TEXT("identity transform needs no per-point copies/callbacks"),Calls,0);
    TestEqual(TEXT("identity keeps displayed point count"),View->ColoredPointBatch->BatchedPoints.Num(),Baseline.Num());
    for(int32 I=0;I<Baseline.Num();++I)
    {
        TestEqual(TEXT("identity preserves display positions"),View->ColoredPointBatch->BatchedPoints[I].Position,Baseline[I].Position);
        TestEqual(TEXT("identity preserves display colors"),View->ColoredPointBatch->BatchedPoints[I].Color,Baseline[I].Color);
    }
    View->SetPointPreviewTransform([&](FVirtualLidarPoint& P,double){++Calls;P.WorldLocation.Z+=5;});
    View->SetPointPreviewTransformNeeded([](double){return true;});Calls=0;View->RefreshPointPreview();
    TestEqual(TEXT("changed geometry still applies every selected-point adapter"),Calls,8);
    for(int32 I=0;I<8;++I)
    {
        TestEqual(TEXT("nonidentity transform updates display copy"),View->ColoredPointBatch->BatchedPoints[I].Position,Baseline[I].Position+FVector(0,0,5));
        TestEqual(TEXT("display transforms never mutate acquired snapshot"),(*Snapshot->Points)[I].WorldLocation,Points[I].WorldLocation);
    }
    View->SetPointPreviewTransformNeeded([](double){return false;});
    View->SetPointPreviewTransform([&](FVirtualLidarPoint&,double){++Calls;});Calls=0;View->RefreshPointPreview();
    TestEqual(TEXT("legacy setter clears stale optional preflight"),Calls,8);
    TestEqual(TEXT("all preview refreshes add no acquisition"),Scan->GetRuntimeStatus().FrameId,FrameId);
    View->SetPointPreviewTransform({});Actor->Destroy();return true;
}
#endif
