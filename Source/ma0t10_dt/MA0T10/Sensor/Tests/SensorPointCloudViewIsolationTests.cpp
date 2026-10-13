#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPointCloudViewIsolationTest,"MA0T10.SensorWorkspace.PointCloudViewIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPointCloudViewIsolationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* First=World->SpawnActor<APlayerController>();auto* Second=World->SpawnActor<APlayerController>();
	TestNotNull(TEXT("first controller fixture spawned"),First);TestNotNull(TEXT("second controller fixture spawned"),Second);
	if(!First||!Second){if(First)First->Destroy();if(Second)Second->Destroy();return false;}
	// Editor maps do not run gameplay controller initialization/registration.
	// Match the runtime World controller registry before exercising the production lookup.
	World->AddController(First);World->AddController(Second);
	auto* View=World->GetFirstPlayerController();
	TestNotNull(TEXT("fixture has a concrete player view"),View);
	if(!View){World->RemoveController(First);World->RemoveController(Second);First->Destroy();Second->Destroy();return false;}
	auto* Other=View==First?Second:First;
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();Manager->bPointCloudOnlyHideWorld=true;
	auto* Target=World->SpawnActor<AStaticMeshActor>();auto* Foreign=World->SpawnActor<AStaticMeshActor>();auto* Later=World->SpawnActor<AStaticMeshActor>();
	auto* Primitive=Target->GetStaticMeshComponent();auto* ForeignPrimitive=Foreign->GetStaticMeshComponent();
	Primitive->SetVisibility(true);Primitive->SetHiddenInGame(false);
	ForeignPrimitive->SetVisibility(false);ForeignPrimitive->SetHiddenInGame(true);
	View->HiddenPrimitiveComponents.Add(ForeignPrimitive);
	Other->HiddenPrimitiveComponents.Add(ForeignPrimitive);
	View->HiddenActors.Add(Foreign);
	auto* CaptureOwner=World->SpawnActor<AActor>();
	auto* Capture=NewObject<USceneCaptureComponent2D>(CaptureOwner);Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->RegisterComponent();
	Capture->HiddenActors.Add(Foreign);Capture->HiddenComponents.Add(ForeignPrimitive);
	const int32 CaptureActorCount=Capture->HiddenActors.Num(),CaptureComponentCount=Capture->HiddenComponents.Num();
	Manager->SetPointCloudOnlyMode(true);
	TestTrue(TEXT("only the selected player's view hides the target"),View->HiddenPrimitiveComponents.Contains(Primitive));
	TestFalse(TEXT("other player view remains unchanged"),Other->HiddenPrimitiveComponents.Contains(Primitive));
	TestTrue(TEXT("primitive global visibility remains enabled for GPU depth"),Primitive->IsVisible());
	TestFalse(TEXT("primitive HiddenInGame remains false"),Primitive->bHiddenInGame);
	TestFalse(TEXT("actor global hidden flag unchanged"),Target->IsHidden());
	TestFalse(TEXT("pre-existing globally invisible primitive remains invisible"),ForeignPrimitive->IsVisible());
	TestTrue(TEXT("pre-existing global hidden flag retained"),ForeignPrimitive->bHiddenInGame);
	TestEqual(TEXT("sensor capture actor exclusions unchanged"),Capture->HiddenActors.Num(),CaptureActorCount);
	TestEqual(TEXT("sensor capture component exclusions unchanged"),Capture->HiddenComponents.Num(),CaptureComponentCount);
	TestFalse(TEXT("target not injected into scene-capture hidden list"),Capture->HiddenComponents.Contains(Primitive));
	// Another system may append another copy or a different component during point-only mode.
	View->HiddenPrimitiveComponents.Add(Primitive);
	View->HiddenPrimitiveComponents.Add(Later->GetStaticMeshComponent());
	Manager->SetPointCloudOnlyMode(false);
	TestEqual(TEXT("restore removes only one owned matching entry"),View->HiddenPrimitiveComponents.FilterByPredicate([Primitive](const auto& P){return P.Get()==Primitive;}).Num(),1);
	TestTrue(TEXT("pre-existing foreign hidden entry preserved"),View->HiddenPrimitiveComponents.Contains(ForeignPrimitive));
	TestTrue(TEXT("later foreign hidden entry preserved"),View->HiddenPrimitiveComponents.Contains(Later->GetStaticMeshComponent()));
	TestTrue(TEXT("foreign actor hidden list preserved"),View->HiddenActors.Contains(Foreign));
	TestTrue(TEXT("restore never modifies global target visibility"),Primitive->IsVisible()&&!Primitive->bHiddenInGame);
	Manager->SetPointCloudOnlyMode(true);Manager->SetPointCloudOnlyMode(false);
	TestEqual(TEXT("repeated toggle does not remove existing shared hidden entry"),View->HiddenPrimitiveComponents.FilterByPredicate([Primitive](const auto& P){return P.Get()==Primitive;}).Num(),1);
	Manager->Destroy();Target->Destroy();Foreign->Destroy();Later->Destroy();CaptureOwner->Destroy();
	World->RemoveController(First);World->RemoveController(Second);First->Destroy();Second->Destroy();
	return true;
}
#endif
