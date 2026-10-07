#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabTrackReferenceActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

namespace
{
FSlabScenarioDataPtr ResetFixture(ESlabInputUnit Unit)
{
	const FString Json=FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root);
	const double Scale=Unit==ESlabInputUnit::Millimeters?10:Unit==ESlabInputUnit::Meters?.01:1;
	for(const auto& Item:Root->GetArrayField(TEXT("DATA_MAP")))
	{
		const auto Row=Item->AsObject();
		Row->SetNumberField(TEXT("slab_len"),1083*Scale);
		Row->SetNumberField(TEXT("slab_wth"),110*Scale);
		Row->SetNumberField(TEXT("slab_thk"),25*Scale);
	}
	FString Modified;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Modified));
	FSlabScenarioDataPtr Data;FString Error;FSlabScenarioCodec::Parse(Modified,Data,Error,true);return Data;
}

class FSlabResetApiCheck : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;double Start=FPlatformTime::Seconds();
public:
	explicit FSlabResetApiCheck(FAutomationTestBase* In):Test(In){}
	bool Update() override
	{
		UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
		if(!World||!World->GetGameInstance()){if(FPlatformTime::Seconds()-Start>15){Test->AddError(TEXT("Reset API PIE timeout"));return true;}return false;}
		auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
		auto* Archive=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
		FString Reason;
		for(const auto Unit:{ESlabInputUnit::Centimeters,ESlabInputUnit::Millimeters,ESlabInputUnit::Meters})
		{
			const FTransform Placement(FRotator(8,47,-3),FVector(137,-211,63),FVector(1.3,.9,1.1));
			auto* Track=World->SpawnActor<ASlabTrackReferenceActor>(FVector(2200,550,170),FRotator(0,-35,0));
			auto* Slab=World->SpawnActorDeferred<ASlabActor>(ASlabActor::StaticClass(),Placement);
			Slab->ReceiverId=FGuid::NewGuid().ToString();Slab->TrackReference=Track;Slab->DimensionUnit=Unit;
			Slab->SensorOutputs=FVirtualSlabSensorOutputSelection::ObservationOnly();
			Test->TestFalse(TEXT("before BeginPlay reset rejected"),Slab->CanResetToInitialPlacement(Reason));
			Slab->FinishSpawning(Placement);
			auto* Charts=CreateWidget<USlabChartsPanelWidget>(World,USlabChartsPanelWidget::StaticClass());Charts->TakeWidget();Charts->BindSlabActor(Slab);
			auto* Progress=CreateWidget<USlabProgressPanelWidget>(World,USlabProgressPanelWidget::StaticClass());Progress->BindSlabActor(Slab);
			const auto Data=ResetFixture(Unit);Test->TestTrue(TEXT("600 row fixture parsed"),Data.IsValid());
			Test->TestTrue(TEXT("new bulk starts observation"),Slab->ReceiveScenario(Data));
			Charts->RefreshChartData();
			for(int32 I=0;I<3;++I)Test->TestTrue(TEXT("chart has scenario data before reset"),Charts->GetChartWidget(I)&&Charts->GetChartWidget(I)->GetRevealedSampleCount()>0);
			Test->TestFalse(TEXT("progress API uses the same busy guard"),Progress->CanResetSlabToInitialPlacement(Reason));
			Test->TestFalse(TEXT("first scenario pose differs from level placement"),Slab->GetActorLocation().Equals(Placement.GetLocation(),.1));
			Test->TestFalse(TEXT("playing reset rejected"),Slab->ResetToInitialPlacement(Reason));
			Test->TestTrue(TEXT("pause accepted"),Slab->SetSimulationPaused(true));
			Test->TestFalse(TEXT("paused reset rejected"),Slab->CanResetToInitialPlacement(Reason));
			Slab->SetSimulationPaused(false);Slab->AdvanceSimulation(30);
			Test->TestFalse(TEXT("drain reset rejected"),Slab->CanResetToInitialPlacement(Reason));Session->Tick(0);
			const auto MeshScale=Slab->SlabMesh->GetRelativeScale3D();auto* Material=Slab->SlabMesh->GetMaterial(0);const auto Mesh=Slab->SlabMesh->GetStaticMesh();
			Slab->SetActorScale3D(FVector(1.2,.8,1.4));const auto ActorScale=Slab->GetActorScale3D();
			const auto BeforeSession=Session->GetSlabSensorSessionStatus();const int32 Stored=Archive->GetScenarios().Num();
			const int32 HelperCount=Slab->FindComponentByClass<USlabVisualizationComponent>()->GetOwnedHelperCount();
			Test->TestTrue(TEXT("finished reset accepted through progress API"),Progress->ResetSlabToInitialPlacement(Reason));
			for(int32 I=0;I<3;++I){auto* Chart=Charts->GetChartWidget(I);Test->TestEqual(TEXT("all chart samples hidden after reset"),Chart->GetRevealedSampleCount(),0);Test->TestTrue(TEXT("chart hover cleared"),Chart->GetHoverSummary().IsEmpty());}
			Charts->BindSlabActor(Slab);for(int32 I=0;I<3;++I)Test->TestEqual(TEXT("chart rebind cannot reveal old results"),Charts->GetChartWidget(I)->GetRevealedSampleCount(),0);
			Test->TestEqual(TEXT("reset reuses diagnostic pool"),Slab->FindComponentByClass<USlabVisualizationComponent>()->GetOwnedHelperCount(),HelperCount);
			TArray<UTextRenderComponent*> Labels;Slab->GetComponents(Labels);for(auto* Label:Labels)Test->TestFalse(TEXT("old 3D analysis labels hidden"),Label->IsVisible());
			Test->TestTrue(TEXT("exact initial world location"),Slab->GetActorLocation().Equals(Placement.GetLocation(),.1));
			Test->TestTrue(TEXT("exact initial world rotation"),Slab->GetActorQuat().AngularDistance(Placement.GetRotation())<=FMath::DegreesToRadians(.01));
			Test->TestEqual(TEXT("current actor scale retained"),Slab->GetActorScale3D(),ActorScale);
			Test->TestEqual(TEXT("current mesh dimensions retained"),Slab->SlabMesh->GetRelativeScale3D(),MeshScale);
			Test->TestEqual(TEXT("surface material retained"),Slab->SlabMesh->GetMaterial(0),Material);
			Test->TestTrue(TEXT("mesh retained"),Slab->SlabMesh->GetStaticMesh()==Mesh);
			Test->TestEqual(TEXT("idle after reset"),Slab->GetSimulationStatus().State,ESlabSimulationState::Idle);
			Test->TestEqual(TEXT("no invented source frame"),Slab->GetSimulationStatus().FrameNo,int64(INDEX_NONE));
			Test->TestTrue(TEXT("no new execution UUID"),Slab->GetSimulationStatus().RunUUID.IsEmpty());
			Test->TestEqual(TEXT("archive retained"),Archive->GetScenarios().Num(),Stored);
			Test->TestEqual(TEXT("sensor session identity unchanged"),Session->GetSlabSensorSessionStatus().RunId,BeforeSession.RunId);
			Test->TestEqual(TEXT("sensor session outcome unchanged"),Session->GetSlabSensorSessionStatus().State,BeforeSession.State);
			const auto Restored=Slab->GetActorTransform();Slab->AdvanceSimulation(1);Slab->FindComponentByClass<USlabMotionComponent>()->TickComponent(.5,ELevelTick::LEVELTICK_All,nullptr);
			Test->TestTrue(TEXT("old motion cannot move reset slab"),Slab->GetActorTransform().Equals(Restored));
			Test->TestTrue(TEXT("repeated reset accepted"),Slab->ResetToInitialPlacement(Reason));
			Test->TestFalse(TEXT("same UUID still deduplicated"),Slab->ReceiveScenario(Data));
			const auto Partial=ResetFixture(Unit);Test->TestTrue(TEXT("partial scenario starts"),Slab->ReceiveScenario(Partial));
			Slab->AdvanceSimulation(2);Slab->StopSimulation();Session->Tick(0);
			Test->TestTrue(TEXT("early stop cleanup permits reset"),Slab->ResetToInitialPlacement(Reason));
			const auto Next=ResetFixture(Unit);Test->TestTrue(TEXT("new UUID autoplays after reset"),Slab->ReceiveScenario(Next));
			Charts->RefreshChartData();for(int32 I=0;I<3;++I)Test->TestTrue(TEXT("next run repopulates charts"),Charts->GetChartWidget(I)->GetRevealedSampleCount()>0);
			Slab->StopSimulation();Session->Tick(0);
			Test->TestTrue(TEXT("async JSON request accepted"),Slab->SubmitScenarioJson(Data->OriginalJson));
			Test->TestFalse(TEXT("actor parse preparation blocks reset"),Slab->CanResetToInitialPlacement(Reason));
			Slab->StopSimulation();Test->TestTrue(TEXT("cancelled parse cannot block reset"),Slab->ResetToInitialPlacement(Reason));
			Slab->Destroy();Track->Destroy();
		}
		return true;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabResetApiTest,"MA0T10.SlabResetPlacement.ActorLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabResetApiTest::RunTest(const FString&)
{
	FAutomationEditorCommonUtils::CreateNewMap();ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FSlabResetApiCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
