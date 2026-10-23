#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Types/SlateAttributeMetaData.h"

namespace
{
void UxrCapabilityText(const TSharedRef<SWidget>& Widget,TArray<FString>& Text)
{
    FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(!Widget->GetVisibility().IsVisible())return;
    FSlateAttributeMetaData::UpdateExceptVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(Widget->GetType()==TEXT("STextBlock"))Text.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)UxrCapabilityText(Children->GetChildAt(I),Text);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrSlabCapabilityIsolation,"MA0T10.UXRP1P2.SlabCapabilityIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrSlabCapabilityIsolation::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* Coordinator=World->SpawnActor<AVirtualSensorCoordinator>();Workspace->SetCoordinator(Coordinator);
    auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Lidar->Tags.Add(TEXT("SensorTestManaged"));Lidar->StopSensor();Coordinator->RegisterSensorActor(Lidar);Coordinator->SelectSensorActor(Lidar);
    auto* Slab=World->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("SensorTestManaged"));
    for(bool Enabled:{false,true})
    {
        Workspace->SetSlabToolsEnabled(Enabled);
        auto* Preview=NewObject<UVirtualSensorMonitorPanelWidget>(World);Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Preview);
        TestEqual(TEXT("Owned preview Slab summary follows the same capability"),Preview->GetSlabAnalysisSummaryText().IsEmpty(),!Enabled);
        TSharedPtr<SWidget> ReplayRoot;
        if(Enabled)
        {
            auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World);Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Replay,Replay);Replay->Initialize();ReplayRoot=Replay->TakeWidget();
        }
        auto* Data=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Data,Data);
        Data->BindSensorManager(Coordinator);Data->BindScenarioSlabActor(Slab);Data->Initialize();Data->SetVisibility(ESlateVisibility::Visible);
        const auto Root=Data->TakeWidget();Data->OpenOwnedTab(ESensorOwnedDataTab::Transfer);
        // A stale scenario/replay selection must not override the current world's capability.
        Data->bOwnedScenarioStreamView=true;Data->bOwnedReplayPreflight=true;Data->LastNativeStatusRefreshSeconds=-1;Data->TickOwnedUi(1);
        TestEqual(TEXT("Scenario purpose follows the world capability, including stale replay selection"),Data->IsOwnedScenarioStreamView(),Enabled);
        TestEqual(TEXT("Coil mode never prepares a replay owner"),Data->bOwnedReplayOwnerRequested,Enabled);
        if(!Enabled)
        {
            TestTrue(TEXT("Replay panel is not created in the coil world"),Workspace->GetOwnedPanel(ESensorToolPanelRole::Replay)==nullptr);
            TestFalse(TEXT("Manual preflight never reports a missing Slab scenario owner"),Data->CachedPcdPreflightSummary.Contains(TEXT("시나리오")));
            TestTrue(TEXT("Manual preflight never offers a scenario/output-policy fix"),Data->OwnedPreflightFix!=3&&Data->OwnedPreflightFix!=4);
        }
        Root->SlatePrepass(1);TArray<FString> Transfer;UxrCapabilityText(Root,Transfer);
        if(Enabled)
        {
            TestTrue(TEXT("Slab world retains new scenario and replay cards"),Transfer.Contains(TEXT("신규 시나리오"))&&Transfer.Contains(TEXT("보관 목록 재생")));
            TestTrue(TEXT("Slab world retains all three preflight targets"),Transfer.Contains(TEXT("신규"))&&Transfer.Contains(TEXT("재생"))&&Transfer.Contains(TEXT("직접")));
        }
        else
        {
            for(const auto& Text:Transfer)TestFalse(TEXT("No visible Slab/scenario/replay wording in coil transfer: ")+Text,Text.Contains(TEXT("Slab"))||Text.Contains(TEXT("시나리오"))||Text.Contains(TEXT("재생")));
            TestTrue(TEXT("General PCD transmission and bandwidth controls remain present"),Transfer.Contains(TEXT("PCD 직접 전송"))&&Transfer.Contains(TEXT("PCD 전송 설정"))&&Transfer.Contains(TEXT("LZF 압축")));
        }
        Data->OpenOwnedTab(ESensorOwnedDataTab::Results);
        TestEqual(TEXT("Run-result expansion follows the same capability"),Data->AreScenarioRunResultsExpanded(),Enabled);
        if(!Enabled)
        {
            Data->RunResultsText=TEXT("hidden run details sentinel");Data->RefreshNativeText();
            TestEqual(TEXT("Hidden Slab run details are not formatted"),Data->RunResultsText,FString(TEXT("hidden run details sentinel")));
            Root->SlatePrepass(1);TArray<FString> Results;UxrCapabilityText(Root,Results);
            for(const auto& Text:Results)TestFalse(TEXT("No Slab scenario wording leaks from the generic result tab"),Text.Contains(TEXT("Slab"))||Text.Contains(TEXT("시나리오"))||Text.Contains(TEXT("보관 목록 재생")));
        }
        Data->OpenOwnedTab(ESensorOwnedDataTab::FileSave);Root->SlatePrepass(1);TArray<FString> File;UxrCapabilityText(Root,File);
        TestTrue(TEXT("File saving remains available in both world types"),File.Contains(TEXT("새 프레임 저장")));
        TestFalse(TEXT("Showing UI does not start general PCD transmission"),Coordinator->StreamPublisherComponent->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId()));
        Workspace->UnregisterPanel(Data);
        Workspace->UnregisterPanel(Preview);
        if(auto* Replay=Workspace->GetOwnedPanel(ESensorToolPanelRole::Replay))Workspace->UnregisterPanel(Replay);
    }
    Workspace->SetSlabToolsEnabled(true);Coordinator->UnregisterSensorActor(Lidar);Lidar->Destroy();Slab->Destroy();Coordinator->Destroy();return true;
}
#endif
