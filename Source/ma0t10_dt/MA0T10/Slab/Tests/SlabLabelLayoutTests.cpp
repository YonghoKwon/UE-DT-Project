#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Slab/SlabLabelLayout.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabLabelLayoutTest,"MA0T10.UI02.PriorityExclusionsAndDistance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabLabelLayoutTest::RunTest(const FString&)
{
    for(const FVector2D View:{FVector2D(1280,720),FVector2D(1920,1080)})
    {
        const FBox2D Safe(FVector2D(12,12),View-FVector2D(12,12));
        TArray<FBox2D> Blocks={FBox2D(FVector2D::ZeroVector,FVector2D(372,View.Y)),FBox2D(FVector2D(View.X-300,0),FVector2D(View.X,230)),FBox2D(FVector2D(View.X-420,View.Y-330),View)};
        FSlabLabelCandidate C[5];for(auto& Item:C){Item.Anchor=FVector2D(420,200);Item.HalfSize=FVector2D(90,16);Item.bEnabled=true;}
        const int32 Order[5]={3,0,1,2,4};FSlabLabelPlacement R[5];SlabLabelLayout::Place(C,Order,Safe,Blocks,R);
        TestTrue(TEXT("highest priority material label retains a safe slot"),R[3].bVisible);
        int32 Count=0;
        for(int32 I=0;I<5;++I)if(R[I].bVisible)
        {
            ++Count;TestTrue(TEXT("entire label inside viewport"),Safe.IsInside(R[I].Rect.Min)&&Safe.IsInside(R[I].Rect.Max));
            for(const auto& B:Blocks)TestFalse(TEXT("label avoids sidebar and both cards"),R[I].Rect.Intersect(B));
            for(int32 J=0;J<I;++J)if(R[J].bVisible)TestFalse(TEXT("no accepted label overlaps another"),R[I].Rect.Intersect(R[J].Rect));
        }
        TestTrue(TEXT("bounded crowding suppression"),Count>0&&Count<=5);
        Blocks.Add(Safe);SlabLabelLayout::Place(C,Order,Safe,Blocks,R);
        for(const auto& Item:R)TestFalse(TEXT("fully occupied view suppresses every label"),Item.bVisible);
        C[3].HalfSize=View;Blocks.Reset();SlabLabelLayout::Place(C,Order,Safe,Blocks,R);
        TestFalse(TEXT("oversize label never clips into UI"),R[3].bVisible);
    }
    TestEqual(TEXT("near fade full contrast"),SlabLabelLayout::DistanceFade(1000),1.f);
    TestTrue(TEXT("far fade decreases"),SlabLabelLayout::DistanceFade(8000)<SlabLabelLayout::DistanceFade(4000));
    TestEqual(TEXT("very far hides"),SlabLabelLayout::DistanceFade(15000),0.f);
    TestEqual(TEXT("behind camera hides"),SlabLabelLayout::DistanceFade(-1),0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabLabelLifecycleTest,"MA0T10.UI02.DetailSelectionAndLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabLabelLifecycleTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();
    auto* V=Slab->FindComponentByClass<USlabVisualizationComponent>();
    const int32 Pool=V->GetOwnedHelperCount();const auto Original=Slab->GetAnalysisDisplaySettings();
    TestFalse(TEXT("detail labels initially off"),Slab->GetDetailedDiagnosticLabels());
    for(int32 I=0;I<20;++I){Slab->SetDetailedDiagnosticLabels(true);Slab->SetDiagnosticHelpersVisible(false);Slab->SetDiagnosticHelpersVisible(true);TestTrue(TEXT("master hide preserves detail choice"),Slab->GetDetailedDiagnosticLabels());Slab->SetDetailedDiagnosticLabels(false);}
    TestEqual(TEXT("toggles never allocate new helpers"),V->GetOwnedHelperCount(),Pool);
    const auto After=Slab->GetAnalysisDisplaySettings();
    TestTrue(TEXT("existing individual display settings unchanged"),Original.bMargins==After.bMargins&&Original.bStatus==After.bStatus&&Original.TextHeightPixels==After.TextHeightPixels);
    TestEqual(TEXT("continuous update interval at most 5Hz"),V->PrimaryComponentTick.TickInterval,.2f);
    Slab->DispatchBeginPlay();V->EndPlay(EEndPlayReason::EndPlayInEditor);Slab->SetDetailedDiagnosticLabels(true);V->SetLabelExclusionRects(Slab,{FBox2D(FVector2D::ZeroVector,FVector2D(372,720))});
    TestEqual(TEXT("late UI callback cannot recreate ended primitives"),V->GetOwnedHelperCount(),0);
    Slab->Destroy();return true;
}
#endif
