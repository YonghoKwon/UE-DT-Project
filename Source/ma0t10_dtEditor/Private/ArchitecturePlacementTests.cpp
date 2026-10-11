#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerControllerTest.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorPlacementInputOwner.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArchitecturePlacementTest,"MA0T10.Architecture.PlacementInputView",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArchitecturePlacementTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* PC=World->SpawnActor<ADxPlayerControllerTest>();
    auto* G=World->SpawnActor<AVirtualSensorTransformGizmoActor>();
    PC->RegisterPlacementInputOwner(G);
    TestNull(TEXT("legacy inactive owner filtered"),PC->GetPlacementInputOwner());
    TestNull(TEXT("interface inactive owner filtered identically"),PC->GetPlacementInputView());
    G->ConfigureResponsivePlacement(true,EVirtualSensorPlacementInputMode::Continuous);
    G->SetManipulationEnabled(true);
    TestEqual(TEXT("legacy concrete getter preserved"),PC->GetPlacementInputOwner(),G);
    TestTrue(TEXT("same actor owns new input view"),PC->GetPlacementInputView()==static_cast<IVirtualSensorPlacementInputOwner*>(G));
    TestEqual(TEXT("focus predicate unchanged"),PC->GetPlacementInputView()->HasPlacementInputFocus(),G->IsInputFocusOwned());
    TestEqual(TEXT("observer predicate unchanged"),PC->GetPlacementInputView()->IsPlacementObserverNavigationActive(),G->IsObserverNavigationActive());
    PC->GetPlacementInputView()->EndPlacementObserverNavigation();
    TestFalse(TEXT("observer end routed without starting navigation"),G->IsObserverNavigationActive());
    G->SetManipulationEnabled(false);
    TestNull(TEXT("interface releases disabled manipulation"),PC->GetPlacementInputView());
    PC->UnregisterPlacementInputOwner(G);
    TestNull(TEXT("unregister clears legacy owner"),PC->GetPlacementInputOwner());
    TestNull(TEXT("unregister clears interface owner"),PC->GetPlacementInputView());
    PC->RegisterPlacementInputOwner(G);G->Destroy();
    TestNull(TEXT("destroyed weak owner cannot route input"),PC->GetPlacementInputView());
    return true;
}
#endif
