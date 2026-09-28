#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorWorkspaceForeignFixture.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "Widgets/Text/STextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorOwnedPresentationTest, "MA0T10.Slab.UI.OwnedPresentationIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSensorOwnedPresentationTest::RunTest(const FString&)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* Owned=NewObject<USlabProgressPanelWidget>(World);
    auto* Foreign=NewObject<USensorWorkspaceForeignFixture>(World);
    const auto GlobalColor=FVirtualSensorUiStyle::PanelBackground;
    const FButtonStyle* GlobalButton=&FVirtualSensorUiStyle::ButtonStyle();
    const auto ForeignSize=Foreign->GetEffectivePanelSize();
    TestTrue(TEXT("unregistered keeps legacy style"),Owned->GetToolPanelColor().Equals(GlobalColor));
    TestTrue(TEXT("explicit panel registration"),Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress,Owned));
    TestTrue(TEXT("owned background is near opaque"),Owned->GetToolPanelColor().A>=.99f);
    TestTrue(TEXT("owned buttons distinct"),&Owned->GetToolButtonStyle()!=GlobalButton);
    TestTrue(TEXT("foreign buttons unchanged"),&Foreign->GetToolButtonStyle()==GlobalButton);
    TSharedPtr<STextBlock> Temporary;
    for(int32 I=0;I<50;++I)
    {
        Temporary=SNew(STextBlock).Text(FText::FromString(TEXT("row")));
        Owned->RegisterSensorNativeFont(Temporary.ToSharedRef(),STextBlock::FArguments());
        Temporary.Reset();
    }
    Temporary=SNew(STextBlock);
    Owned->RegisterSensorNativeFont(Temporary.ToSharedRef(),STextBlock::FArguments());
    TestEqual(TEXT("expired row font callbacks pruned"),Owned->GetRegisteredFontControlCount(),1);
    Workspace->SetOwnedPanelFontScale(1.5f);
    TestEqual(TEXT("owned role font scales"),Temporary->GetFont().Size,24.0f);
    TestTrue(TEXT("global shared color unchanged"),FVirtualSensorUiStyle::PanelBackground.Equals(GlobalColor));
    TestTrue(TEXT("foreign background unchanged"),Foreign->GetToolPanelColor().Equals(GlobalColor));
    TestEqual(TEXT("foreign geometry unchanged"),Foreign->GetEffectivePanelSize(),ForeignSize);
    TestEqual(TEXT("foreign font scale unchanged"),Foreign->GetSensorToolFontScale(),1.0f);
    Workspace->UnregisterPanel(Owned);
    return true;
}
#endif
