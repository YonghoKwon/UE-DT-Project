#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabMotionOwnershipTest,"MA0T10.SlabScenario.MotionComponent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabMotionOwnershipTest::RunTest(const FString&)
{
	FSlabScenarioDataPtr Data; FString Error;
	if(!FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(),Data,Error)) return false;
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* Owner=World->SpawnActor<AActor>();
	auto* Root=NewObject<USceneComponent>(Owner); Root->SetMobility(EComponentMobility::Movable); Owner->SetRootComponent(Root); Root->RegisterComponent();
	auto* Motion=NewObject<USlabMotionComponent>(Owner); Motion->RegisterComponent();
	const FTransform Track(FRotator(0,30,0),FVector(200,100,20)); const FVector Size(1083,110,25);
	int32 Calls=0; bool bSawEnd=false;
	Motion->OnPoseApplied.BindLambda([&](const FSlabScenarioRow& Row,int32 Index,double Time,bool bEnd)
	{
		++Calls; bSawEnd|=bEnd;
		TestTrue(TEXT("component applies pose before notifying owner"),Owner->GetActorTransform().Equals(USlabMotionComponent::BuildPose(Row,Track,Size,ESlabInputUnit::Centimeters),1.e-6));
		TestEqual(TEXT("component owns continuous clock"),Motion->GetPlaybackTime(),Time);
		TestEqual(TEXT("original row index remains source-frame identity"),Data->Rows[Index].FrameNo,Row.FrameNo);
	});
	TestTrue(TEXT("motion runs without ASlabActor or sensor subsystem"),Motion->StartPlayback(Data,Track,Size,ESlabInputUnit::Centimeters));
	TestEqual(TEXT("initial pose applied synchronously once"),Calls,1);
	Motion->AdvancePlayback(0.025);
	const FVector Local=Track.InverseTransformPositionNoScale(Owner->GetActorLocation());
	TestTrue(TEXT("component owns sub-frame interpolation"),FMath::IsNearlyEqual(Local.X,0.775,1.e-6));
	TestTrue(TEXT("component pause accepted"),Motion->SetPlaybackPaused(true));
	Motion->AdvancePlayback(1); TestEqual(TEXT("pause retains component clock"),Motion->GetPlaybackTime(),0.025);
	TestTrue(TEXT("component resume accepted"),Motion->SetPlaybackPaused(false)); Motion->AdvancePlayback(30);
	TestTrue(TEXT("component emits terminal pose"),bSawEnd); TestFalse(TEXT("component stops at duration"),Motion->IsPlaybackRunning());
	TestEqual(TEXT("duration clamps to exact thirty seconds"),Motion->GetPlaybackTime(),30.0);
	const FTransform Finished=Owner->GetActorTransform(); Motion->AdvancePlayback(1); TestTrue(TEXT("finished component cannot move again"),Owner->GetActorTransform().Equals(Finished));
	Motion->OnPoseApplied.BindLambda([&](const FSlabScenarioRow&,int32,double,bool){Motion->StopPlayback();});
	TestFalse(TEXT("callback rejection cannot re-enable ticking"),Motion->StartPlayback(Data,Track,Size,ESlabInputUnit::Centimeters));
	TestFalse(TEXT("rejected start stays stopped"),Motion->IsPlaybackRunning()); TestFalse(TEXT("rejected start tick disabled"),Motion->IsComponentTickEnabled());
	Motion->DestroyComponent(); Owner->Destroy(); return true;
}
#endif
