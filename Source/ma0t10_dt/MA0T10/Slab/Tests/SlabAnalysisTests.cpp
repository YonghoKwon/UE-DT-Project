#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabTrackReferenceActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"
#include "TimerManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabPlainSurfaceTest,"MA0T10.SlabAnalysis.PlainMaterial",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabPlainSurfaceTest::RunTest(const FString&)
{
	const FString Plain=TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain");
	TestEqual(TEXT("new actor defaults plain"),GetDefault<ASlabActor>()->GetEffectiveSurfaceMaterialPath(),Plain);
	TestEqual(TEXT("unset uses plain"),ASlabActor::ResolveSurfaceMaterialPath(FSoftObjectPath()).ToString(),Plain);
	TestEqual(TEXT("old owned stock resolves without editing map"),ASlabActor::ResolveSurfaceMaterialPath(FSoftObjectPath(TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface.M_SlabSurface"))).ToString(),Plain);
	const FSoftObjectPath Custom(TEXT("/Game/Customer/Steel.M_CustomSteel"));
	TestEqual(TEXT("user material preserved exactly"),ASlabActor::ResolveSurfaceMaterialPath(Custom),Custom);
	auto* Material=LoadObject<UMaterial>(nullptr,*Plain);
	TestNotNull(TEXT("actual plain material asset loads"),Material);
	if(Material)
	{
		TSet<FName> Parameters;
		TestTrue(TEXT("plain material contains an actual graph"),Material->GetExpressions().Num()>0);
		for(UMaterialExpression* Expression:Material->GetExpressions()) if(Expression)
		{
			const FString Class=Expression->GetClass()->GetName();
			TestFalse(TEXT("plain graph has no Noise expression"),Class.Contains(TEXT("Noise")));
			TestFalse(TEXT("plain graph has no WorldPosition expression"),Class.Contains(TEXT("WorldPosition")));
			if(const auto* Scalar=Cast<UMaterialExpressionScalarParameter>(Expression)) Parameters.Add(Scalar->ParameterName);
			if(const auto* Vector=Cast<UMaterialExpressionVectorParameter>(Expression)) Parameters.Add(Vector->ParameterName);
		}
		TestFalse(TEXT("plain graph has no Oxidation parameter"),Parameters.Contains(TEXT("Oxidation")));
		for(const TCHAR* Name:{TEXT("ColdColor"),TEXT("HotColor"),TEXT("Hotness"),TEXT("Roughness"),TEXT("EmissiveStrength")})
			TestTrue(FString::Printf(TEXT("actual graph exposes %s"),Name),Parameters.Contains(Name));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabOverlayIsolationTest,"MA0T10.SlabAnalysis.MeasurementIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabOverlayIsolationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Slab=World->SpawnActor<ASlabActor>();
	Slab->SetDimensionUnit(ESlabInputUnit::Millimeters);
	Slab->DispatchBeginPlay(); // Match the engine lifecycle before testing component EndPlay.
	auto* Visual=Slab->FindComponentByClass<USlabVisualizationComponent>();
	FSlabScenarioRow Row; Row.LeftAngle=-5; FSlabMetrics Metrics; Metrics.LeftAngle=-5; Metrics.bMarginsValid=true; Metrics.MarginLeftCm=-3; Metrics.MarginRightCm=20;
	FSlabSimulationStatus Status; Status.FrameNo=12; Status.MtlNo=TEXT("TEST"); Status.ElapsedSec=.6; Status.DurationSec=30; Status.Progress=.02;
	const FVector Size(1083,110,25); Visual->UpdateAnalysis(Row,Metrics,Status,Size,FTransform::Identity,-70,70,true);
	TestEqual(TEXT("fixed pool includes reusable label backgrounds"),Visual->GetOwnedHelperCount(),USlabVisualizationComponent::MaxLineHelpers+USlabVisualizationComponent::MaxTextHelpers+USlabVisualizationComponent::MaxLabelBackgrounds);
	TArray<UPrimitiveComponent*> Parts; Slab->GetComponents(Parts); int32 Count=0;
	for(auto* P:Parts) if(P!=Slab->SlabMesh)
	{
		++Count; TestEqual(TEXT("helper collision off"),P->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
		TestTrue(TEXT("SceneCapture hides helper"),bool(P->bHiddenInSceneCapture)); TestFalse(TEXT("no helper shadows"),bool(P->CastShadow));
		TestFalse(TEXT("no helper indirect lighting"),bool(P->bAffectDynamicIndirectLighting)); TestFalse(TEXT("no helper distance field"),bool(P->bAffectDistanceFieldLighting));
		TestFalse(TEXT("no helper ray tracing effects"),bool(P->bVisibleInRayTracing)); TestFalse(TEXT("no helper reflection captures"),bool(P->bVisibleInReflectionCaptures)); TestFalse(TEXT("no helper realtime sky captures"),bool(P->bVisibleInRealTimeSkyCaptures));
	}
	TestEqual(TEXT("all owned helper primitives accounted"),Count,Visual->GetOwnedHelperCount());
	UPrimitiveComponent* Centerline=nullptr; for(auto* P:Parts) if(P->GetFName()==TEXT("SlabDiagnosticLine11")) Centerline=P;
	TestNotNull(TEXT("fixed centerline primitive exists"),Centerline);
	if(Centerline)
	{
		const auto FixedPose=Centerline->GetComponentTransform(); Slab->SetActorLocation(FVector(100,0,0));
		Visual->UpdateAnalysis(Row,Metrics,Status,Size,FTransform::Identity,-70,70,true);
		TestTrue(TEXT("centerline remains fixed in Track coordinates"),Centerline->GetComponentTransform().Equals(FixedPose,1.e-6));
		Slab->SetActorLocation(FVector::ZeroVector); Visual->UpdateAnalysis(Row,Metrics,Status,Size,FTransform::Identity,-70,70,true);
	}
	FHitResult Before,After; const FVector Start(0,0,100),End(0,0,-100);
	World->LineTraceSingleByChannel(Before,Start,End,ECC_Visibility);
	FSlabAnalysisDisplaySettings Off; Off.bOutline=Off.bCross=Off.bReferencePose=Off.bCenterline=Off.bYaw=Off.bMargins=Off.bStatus=false;
	Slab->SetAnalysisDisplaySettings(Off); World->LineTraceSingleByChannel(After,Start,End,ECC_Visibility);
	TestTrue(TEXT("CPU trace hits physical slab"),Before.GetComponent()==Slab->SlabMesh);
	TestEqual(TEXT("overlay toggles do not affect CPU hit"),After.GetComponent(),Before.GetComponent());
	TestTrue(TEXT("overlay toggles do not affect CPU depth"),After.ImpactPoint.Equals(Before.ImpactPoint,1.e-4));
	for(int32 I=0;I<100;++I) { Slab->SetAnalysisDisplaySettings(FSlabAnalysisDisplaySettings()); Visual->UpdateAnalysis(Row,Metrics,Status,Size,FTransform::Identity,-70,70,true); Slab->SetAnalysisDisplaySettings(Off); }
	TestEqual(TEXT("repeated options/frames never grow pool"),Visual->GetOwnedHelperCount(),Count);
	TestFalse(TEXT("all text options off disables text tick"),Visual->IsComponentTickEnabled());
	Visual->EndPlay(EEndPlayReason::EndPlayInEditor); TestEqual(TEXT("ending releases owned diagnostics"),Visual->GetOwnedHelperCount(),0);
	Visual->UpdateGeometry(Size); Visual->ConfigureDisplay(FSlabAnalysisDisplaySettings());
	TestEqual(TEXT("late display call cannot recreate ended helper pool"),Visual->GetOwnedHelperCount(),0);
	TestFalse(TEXT("late display call cannot restart ended tick"),Visual->IsComponentTickEnabled());
	Slab->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabCustomSlotMaterialTest,"MA0T10.SlabAnalysis.CustomMaterialSlot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabCustomSlotMaterialTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* Slab=World->SpawnActorDeferred<ASlabActor>(ASlabActor::StaticClass(),FTransform::Identity);
	auto* SlotMaterial=NewObject<UMaterial>(GetTransientPackage());
	Slab->SlabMesh->SetMaterial(0,SlotMaterial); Slab->FinishSpawning(FTransform::Identity);
	TestEqual(TEXT("first construction preserves explicit mesh material without an Actor MID"),Slab->SlabMesh->GetMaterial(0),static_cast<UMaterialInterface*>(SlotMaterial));
	TestEqual(TEXT("effective material reports actual custom slot"),Slab->GetEffectiveSurfaceMaterialPath(),SlotMaterial->GetPathName());
	Slab->RerunConstructionScripts(); Slab->SetHotAppearance(true);
	TestEqual(TEXT("construction and hot toggle do not replace user slot"),Slab->SlabMesh->GetMaterial(0),static_cast<UMaterialInterface*>(SlotMaterial));
	auto* ExplicitMaterial=NewObject<UMaterial>(GetTransientPackage()); Slab->SurfaceMaterial=ExplicitMaterial; Slab->SetHotAppearance(false);
	auto* Applied=Cast<UMaterialInstanceDynamic>(Slab->SlabMesh->GetMaterial(0));
	TestNotNull(TEXT("explicit Actor material remains higher priority"),Applied);
	if(Applied) TestEqual(TEXT("explicit Actor material parent used"),Applied->Parent.Get(),static_cast<UMaterialInterface*>(ExplicitMaterial));
	TestEqual(TEXT("effective explicit property path"),Slab->GetEffectiveSurfaceMaterialPath(),ExplicitMaterial->GetPathName());
	Slab->SlabMesh->SetMaterial(0,SlotMaterial); Slab->SetHotAppearance(true);
	TestEqual(TEXT("explicit property also wins when its existing MID can be reused"),Slab->SlabMesh->GetMaterial(0),static_cast<UMaterialInterface*>(Applied));
	Slab->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabLegacyMaterialTest,"MA0T10.SlabAnalysis.LegacyOwnedMaterial",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabLegacyMaterialTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Slab=World->SpawnActor<ASlabActor>();
	auto* LegacyParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface.M_SlabSurface"));
	TestNotNull(TEXT("legacy stock material asset exists"),LegacyParent); if(!LegacyParent){Slab->Destroy();return false;}
	auto* LegacyMID=UMaterialInstanceDynamic::Create(LegacyParent,Slab);
	TestTrue(TEXT("fixture reproduces old auto-generated MID name"),LegacyMID->GetName().StartsWith(TEXT("MaterialInstanceDynamic")));
	Slab->SlabMesh->SetMaterial(0,LegacyMID);
	const FString Plain=TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain");
	TestEqual(TEXT("serialized legacy generated MID resolves plain without map migration"),Slab->GetEffectiveSurfaceMaterialPath(),Plain);
	Slab->SetHotAppearance(false); auto* Actual=Cast<UMaterialInstanceDynamic>(Slab->SlabMesh->GetMaterial(0));
	TestNotNull(TEXT("owned plain MID restored"),Actual); if(Actual) TestEqual(TEXT("legacy Noise graph replaced at runtime only"),Actual->Parent->GetPathName(),Plain);
	auto* Named=UMaterialInstanceDynamic::Create(LegacyParent,Slab,TEXT("CustomerTint")); Slab->SlabMesh->SetMaterial(0,Named); Slab->SetHotAppearance(true);
	TestEqual(TEXT("custom-named MID retained even with stock parent"),Slab->SlabMesh->GetMaterial(0),static_cast<UMaterialInterface*>(Named));
	auto* CustomParent=NewObject<UMaterial>(GetTransientPackage()); auto* CustomAuto=UMaterialInstanceDynamic::Create(CustomParent,Slab);
	Slab->SlabMesh->SetMaterial(0,CustomAuto); Slab->SetHotAppearance(false);
	TestEqual(TEXT("auto-named MID with user graph is not mistaken for legacy stock"),Slab->SlabMesh->GetMaterial(0),static_cast<UMaterialInterface*>(CustomAuto));
	Slab->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabOtherMapValidationTest,"MA0T10.SlabAnalysis.OtherMapValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabOtherMapValidationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Slab=World->SpawnActor<ASlabActor>();
	Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
	const auto Before=Slab->GetActorTransform(); const auto Unit=Slab->GetDimensionUnit();
	auto V=Slab->ValidateSlabSetup(); TestTrue(TEXT("ordinary map supports observation without Rig"),V.bCanSimulate); TestTrue(TEXT("no selected output needs no broker"),V.bCanSendSelectedOutputs);
	TestTrue(TEXT("missing rail/UI diagnostics explicit"),V.Warnings.Num()>0);
	TestTrue(TEXT("readonly validation keeps transform"),Slab->GetActorTransform().Equals(Before)); TestEqual(TEXT("readonly validation never guesses units"),Slab->GetDimensionUnit(),Unit);
	auto* Duplicate=World->SpawnActor<ASlabActor>(); V=Slab->ValidateSlabSetup(); TestFalse(TEXT("duplicate ReceiverId blocks readiness"),V.bCanSimulate);
	Duplicate->Destroy(); Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection()); V=Slab->ValidateSlabSetup(); TestFalse(TEXT("PCD without coordinator explicit failure"),V.bCanSendSelectedOutputs);
	Slab->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabUiHostRetryTest,"MA0T10.SlabAnalysis.UiHostLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabUiHostRetryTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Host=World->SpawnActor<ASlabSimulationUiHostActor>();
	Host->DispatchBeginPlay();
	TestNull(TEXT("fixture has no local player yet"),World->GetFirstPlayerController()); Host->ShowSimulationPanels();
	TestTrue(TEXT("late player initialization scheduled"),World->GetTimerManager().TimerExists(Host->InitializationRetry));
	for(int32 I=0;I<42;++I) Host->ShowSimulationPanels();
	TestTrue(TEXT("retry attempts explicitly bounded"),Host->InitializationAttempts>=40);
	Host->EndPlay(EEndPlayReason::EndPlayInEditor);
	TestFalse(TEXT("host shutdown clears retry timer"),World->GetTimerManager().TimerExists(Host->InitializationRetry));
	Host->ShowSimulationPanels(); TestFalse(TEXT("ending host cannot restart initialization"),World->GetTimerManager().TimerExists(Host->InitializationRetry));
	Host->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabReadableTextTest,"MA0T10.SlabAnalysis.ReadableWorldText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabReadableTextTest::RunTest(const FString&)
{
	auto Size=[](double Distance){return USlabVisualizationComponent::CalculateReadableTextSize(Distance,90,720,16.0/9.0);};
	TestTrue(TEXT("20m observation yields readable world text"),FMath::IsNearlyEqual(Size(2000),56.25f,.01f));
	TestTrue(TEXT("farther camera increases world text size"),Size(3000)>Size(2000));
	TestEqual(TEXT("near-view minimum bounded"),Size(1),20.0f);TestEqual(TEXT("distant-view maximum bounded"),Size(100000),120.0f);
	TestEqual(TEXT("invalid viewport fallback"),USlabVisualizationComponent::CalculateReadableTextSize(2000,90,0,16.0/9.0),30.0f);
	const float OrthoA=USlabVisualizationComponent::CalculateReadableTextSize(1000,90,720,16.0/9.0,true,3000);
	const float OrthoB=USlabVisualizationComponent::CalculateReadableTextSize(9000,90,720,16.0/9.0,true,3000);
	TestEqual(TEXT("orthographic text is independent of camera distance"),OrthoA,OrthoB);
	const FVector Camera(10,-20,30),Forward(1,0,0),Original=Camera+FVector(2000,300,-150);float Scale=0;
	const FVector Near=USlabVisualizationComponent::PullLabelTowardCamera(Original,Camera,Forward,2000,700,false,Scale)-Camera;
	TestTrue(TEXT("nearer label plane preserves perspective screen X"),FMath::IsNearlyEqual(Near.Y/Near.X,300.0/2000.0,1.e-9));
	TestTrue(TEXT("nearer label plane preserves perspective screen Y"),FMath::IsNearlyEqual(Near.Z/Near.X,-150.0/2000.0,1.e-9));
	TestTrue(TEXT("glyph scale preserves perspective pixel height"),FMath::IsNearlyEqual(double(Scale)/Near.X,1.0/2000.0,1.e-10));
	const FVector Ortho=USlabVisualizationComponent::PullLabelTowardCamera(Original,Camera,Forward,2000,700,true,Scale);
	TestEqual(TEXT("orthographic glyph size unchanged"),Scale,1.0f);TestTrue(TEXT("orthographic projection unchanged"),FMath::IsNearlyEqual(Ortho.Y,Original.Y)&&FMath::IsNearlyEqual(Ortho.Z,Original.Z));
	return true;
}
#endif
