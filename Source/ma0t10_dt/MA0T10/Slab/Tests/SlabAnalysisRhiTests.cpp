#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "ShaderCompiler.h"
#include "RHIGlobals.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"

/** Transient render-world test. No map load/save, subscriptions, extra production captures or credentials. */
class FSlabAnalysisRhiCommand : public IAutomationLatentCommand
{
public:
	explicit FSlabAnalysisRhiCommand(FAutomationTestBase* InTest):Test(InTest){}
	virtual ~FSlabAnalysisRhiCommand() { Cleanup(); }
	bool Update() override
	{
		const double Now=FPlatformTime::Seconds();
		if(!Scene) { if(!CreateFixture()) return Finish(); Started=PhaseStarted=Now; return false; }
		if(Now-Started>90) { Test->AddError(TEXT("Slab RHI isolation test timed out (90s).")); return Finish(); }
		if(Now-Started<2) return false;
		if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()) return false;
		if(Now-PhaseStarted<0.75) return false;
		if(OriginalControlPhase<2)
		{
			// Diagnose the original photometric renderer with OFF->OFF, before changing any visibility.
			Scene->GetWorld()->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
			if(!Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)||Pixels.Num()!=512*512)
			{ Test->AddError(TEXT("Original Camera control readback failed.")); return Finish(); }
			SavePixels(Pixels,FString::Printf(TEXT("camera_original_off_%d.png"),OriginalControlPhase));
			if(OriginalControlPhase==0) OriginalBaseline=MoveTemp(Pixels);
			else
			{
				int32 Changed=0; double Sum=0;
				for(int32 I=0;I<Pixels.Num();++I) {const int32 R=FMath::Abs(int32(Pixels[I].R)-OriginalBaseline[I].R),G=FMath::Abs(int32(Pixels[I].G)-OriginalBaseline[I].G),B=FMath::Abs(int32(Pixels[I].B)-OriginalBaseline[I].B);Changed+=FMath::Max3(R,G,B)>2;Sum+=R+G+B;}
				Report->SetNumberField(TEXT("camera_original_off_off_changed_pixels"),Changed);Report->SetNumberField(TEXT("camera_original_off_off_mean_delta"),Sum/(Pixels.Num()*3.0));
				Test->AddInfo(FString::Printf(TEXT("Original FinalColor OFF/OFF diagnostic: %d changed pixels; no overlay visibility changed."),Changed));
				OriginalBaseline.Reset();
				// Linear scene-color avoids tonemap dithering; stochastic shadow/AO/reflection paths are
				// disabled only on this diagnostic capture. Pixel tolerances below remain unchanged.
				Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;
				Capture->ShowFlags.SetPostProcessing(false);Capture->ShowFlags.SetTonemapper(false);Capture->ShowFlags.SetAmbientOcclusion(false);
				Capture->ShowFlags.SetDynamicShadows(false);Capture->ShowFlags.SetScreenSpaceReflections(false);Capture->ShowFlags.SetLumenReflections(false);Capture->ShowFlags.SetLumenGlobalIllumination(false);
				Report->SetStringField(TEXT("camera_assertion_path"),TEXT("linear scene color; no postprocess/tonemap/AO/dynamic shadows/SSR/Lumen; unchanged <=8 pixels and <=0.05 mean tolerance; positive control required"));
			}
			++OriginalControlPhase; PhaseStarted=Now; return false;
		}
		if(!bCameraCaptured)
		{
			Scene->GetWorld()->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
			if(!Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)||Pixels.Num()!=512*512)
			{ Test->AddError(TEXT("Camera SceneCapture pixel readback failed.")); return Finish(); }
			SavePixels(Pixels);
			if(Phase==0)
			{
				CameraBaseline=MoveTemp(Pixels); int32 NonBlack=0;
				for(const FColor& P:CameraBaseline) NonBlack+=P.R>5||P.G>5||P.B>5;
				Test->TestTrue(TEXT("camera actually renders the physical fixture"),NonBlack>1000); Report->SetNumberField(TEXT("camera_nonblack_pixels"),NonBlack);
			}
			else
			{
				int32 Changed=0; double Sum=0;
				for(int32 I=0;I<Pixels.Num();++I)
				{
					const int32 R=FMath::Abs(int32(Pixels[I].R)-CameraBaseline[I].R),G=FMath::Abs(int32(Pixels[I].G)-CameraBaseline[I].G),B=FMath::Abs(int32(Pixels[I].B)-CameraBaseline[I].B);
					Changed+=FMath::Max3(R,G,B)>2; Sum+=R+G+B;
				}
				Report->SetNumberField(Phase==1?TEXT("camera_deterministic_off_off_changed_pixels"):Phase==2?TEXT("camera_hidden_overlay_changed_pixels"):TEXT("camera_positive_control_changed_pixels"),Changed);
				if(Phase==1||Phase==2)
				{
					Test->TestTrue(Phase==1?TEXT("deterministic OFF/OFF control is stable"):TEXT("Camera excludes visible world annotations"),Changed<=8);
					Test->TestTrue(TEXT("Camera mean channel delta <=0.05"),Sum/(Pixels.Num()*3.0)<=0.05);
				}
				else { Test->TestTrue(TEXT("positive control proves camera would see annotations if capture exclusion were broken"),Changed>20); return Finish(); }
			}
			bCameraCaptured=true;
		}
		if(Phase==1) { NextPhase(Now); return false; }
		auto* Scan=Sensor->ScanComponent.Get();
		if(!bScanRequested)
		{ Scan->StartScan(); Scan->RequestImmediateScheduledScan(); Scan->PrepareScheduledScan(0); bScanRequested=true; }
		Scan->ProcessScheduledScanChunk(1024);
		const auto Frame=Scan->GetLastFrameSnapshot();
		if(!Frame.IsValid()||!Frame->Points.IsValid()||Frame->FrameId==PreviousFrame)
		{
			if(Now-PhaseStarted>30) { Test->AddError(TEXT("GPU depth+semantic frame did not complete.")); return Finish(); }
			return false;
		}
		Scan->StopScan(); PreviousFrame=Frame->FrameId;
		int32 Valid=0,SlabLabels=0,FloorLabels=0;
		for(const auto& P:*Frame->Points) if(P.bHit) {++Valid; SlabLabels+=P.SemanticLabel==TEXT("SlabTest"); FloorLabels+=P.SemanticLabel==TEXT("FloorTest");}
		Test->TestTrue(TEXT("GPU measures physical surfaces"),Valid>500);
		Test->TestTrue(TEXT("GPU semantic buffer classifies slab and floor"),SlabLabels>20&&FloorLabels>20);
		Report->SetNumberField(FString::Printf(TEXT("gpu_phase_%d_slab_points"),Phase),SlabLabels);
		Report->SetNumberField(FString::Printf(TEXT("gpu_phase_%d_floor_points"),Phase),FloorLabels);
		if(Phase==0) DepthBaseline=Frame;
		else
		{
			Test->TestEqual(TEXT("overlay options preserve ray count"),Frame->Points->Num(),DepthBaseline->Points->Num());
			int32 ChangedDepth=0,ChangedSemantic=0; double MaxDeltaCm=0;
			for(int32 I=0;I<FMath::Min(Frame->Points->Num(),DepthBaseline->Points->Num());++I)
			{
				const auto& A=(*DepthBaseline->Points)[I];const auto& B=(*Frame->Points)[I];
				const double D=FVector::Dist(A.WorldLocation,B.WorldLocation); MaxDeltaCm=FMath::Max(MaxDeltaCm,D);
				ChangedDepth+=A.bHit!=B.bHit||D>0.01; ChangedSemantic+=A.SemanticLabel!=B.SemanticLabel;
			}
			Test->TestEqual(TEXT("GPU depth unchanged with annotations visible"),ChangedDepth,0);
			Test->TestEqual(TEXT("GPU semantic identity unchanged with annotations visible"),ChangedSemantic,0);
			Report->SetNumberField(TEXT("gpu_changed_depth_points"),ChangedDepth);Report->SetNumberField(TEXT("gpu_changed_semantic_points"),ChangedSemantic);Report->SetNumberField(TEXT("gpu_max_position_delta_cm"),MaxDeltaCm);
		}
		NextPhase(Now); return false;
	}
private:
	void NextPhase(double Now)
	{
		++Phase; bScanRequested=false; bCameraCaptured=false; PhaseStarted=Now;
		Slab->SetDiagnosticHelpersVisible(Phase>=2);
		if(Phase==3)
		{
			// Temporary positive control belongs only to this disposable fixture.
			TArray<UPrimitiveComponent*> Parts; Slab->GetComponents(Parts);
			for(auto* P:Parts) if(P!=Slab->SlabMesh) P->SetHiddenInSceneCapture(false);
		}
		Scene->GetWorld()->SendAllEndOfFrameUpdates();
	}
	bool CreateFixture()
	{
		Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("scope"),TEXT("transient preview world; Camera and GPU sensor exclusion; not throughput or manual mouse evidence"));
		Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(true));
		UWorld* World=Scene->GetWorld();
		for(const TCHAR* Path:{TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId"),TEXT("/Game/MA0T10/Slab/Materials/M_SlabAnalysisOverlay.M_SlabAnalysisOverlay"),TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain")})
		{ auto* Material=LoadObject<UMaterial>(nullptr,Path); if(!Material){Test->AddError(FString(TEXT("Required owned material missing: "))+Path);return false;} Material->CacheShaders(EMaterialShaderPrecompileMode::Default); }
		auto* Floor=World->SpawnActor<AStaticMeshActor>(); Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->SetActorLocation(FVector(0,0,-5)); Floor->SetActorScale3D(FVector(20,20,.1));
		Slab=World->SpawnActor<ASlabActor>(); Slab->InitialDimensions=FVector(1000,200,25); Slab->SetDimensionUnit(ESlabInputUnit::Centimeters);
		Slab->SetActorLocation(FVector(0,0,12.5)); Slab->SetActorRotation(FRotator(0,-5,0));
		FSlabScenarioRow Row;Row.LeftAngle=-5;FSlabMetrics Metrics=Slab->CalculateMetricsForRow(Row);Metrics.bMarginsValid=true;Metrics.MarginLeftCm=Metrics.MarginRightCm=50;
		FSlabSimulationStatus Status;Status.MtlNo=TEXT("RHI-TEST");Status.FrameNo=30;Status.ElapsedSec=1.5;Status.DurationSec=30;Status.Progress=.05;
		Slab->FindComponentByClass<USlabVisualizationComponent>()->UpdateAnalysis(Row,Metrics,Status,FVector(1000,200,25),FTransform::Identity,-170,170,true,1800);
		Slab->SetDiagnosticHelpersVisible(false);
		Sensor=World->SpawnActor<AVirtualLidarSensorActor>();Sensor->SetActorLocation(FVector(0,0,1000));Sensor->SetActorRotation(FRotator(-90,0,0));
		auto* Scan=Sensor->ScanComponent.Get();Scan->StopScan();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
		Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;Scan->bEnableRangeNoise=false;
		Scan->ViewMode=EVirtualLidarViewMode::ActorClassColor;Scan->bEnableSemanticClassification=true;Scan->SemanticClassRules.Reset();Scan->SetInteractivePreviewMode(false,true);
		FVirtualLidarSemanticClassRule SlabRule;SlabRule.Label=TEXT("SlabTest");SlabRule.ActorClassNames.Add(ASlabActor::StaticClass()->GetFName());Scan->SemanticClassRules.Add(SlabRule);
		FVirtualLidarSemanticClassRule FloorRule;FloorRule.Label=TEXT("FloorTest");FloorRule.ActorClassNames.Add(AStaticMeshActor::StaticClass()->GetFName());Scan->SemanticClassRules.Add(FloorRule);
		Capture=NewObject<USceneCaptureComponent2D>();Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
		Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=1600;
		Capture->ShowFlags.SetAntiAliasing(false);Capture->ShowFlags.SetTemporalAA(false);Capture->ShowFlags.SetMotionBlur(false);Capture->ShowFlags.SetEyeAdaptation(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetAtmosphere(false);
		Capture->TextureTarget=NewObject<UTextureRenderTarget2D>(Capture);Capture->TextureTarget->ClearColor=FLinearColor::Black;Capture->TextureTarget->InitCustomFormat(512,512,PF_B8G8R8A8,false);Capture->TextureTarget->UpdateResourceImmediate(true);
		Scene->AddComponent(Capture,FTransform(FRotator(-90,0,0),FVector(0,0,1500)));
		World->SendAllEndOfFrameUpdates(); return true;
	}
	void SavePixels(const TArray<FColor>& Pixels,const FString& Name=FString())
	{
		const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/SlabAnalysis");IFileManager::Get().MakeDirectory(*Dir,true);
		TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(512,512,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
		FFileHelper::SaveArrayToFile(Png,*(Dir/(Name.IsEmpty()?FString::Printf(TEXT("camera_phase_%d.png"),Phase):Name)));
	}
	bool Finish()
	{
		if(Report.IsValid()) { Report->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/SlabAnalysis");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("rhi_isolation.json"))); }
		Cleanup();return true;
	}
	void Cleanup()
	{
		if(!Scene) return;
		if(Sensor) {Sensor->ScanComponent->StopScan();Sensor->GpuDepthProjectionComponent->CancelAcquisition();Sensor->GpuDepthProjectionComponent->UnregisterComponent();}
		if(Capture) {Scene->RemoveComponent(Capture);Capture=nullptr;}
		DepthBaseline.Reset();Slab=nullptr;Sensor=nullptr;Scene.Reset();
	}
	FAutomationTestBase* Test;
	TUniquePtr<FPreviewScene> Scene;
	ASlabActor* Slab=nullptr;
	AVirtualLidarSensorActor* Sensor=nullptr;
	USceneCaptureComponent2D* Capture=nullptr;
	TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> DepthBaseline;
	TSharedPtr<FJsonObject> Report;
	TArray<FColor> CameraBaseline;
	TArray<FColor> OriginalBaseline;
	double Started=0,PhaseStarted=0;
	int64 PreviousFrame=-1;
	int32 Phase=0;
	int32 OriginalControlPhase=0;
	bool bCameraCaptured=false,bScanRequested=false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabAnalysisRhiTest,"MA0T10.SlabAnalysis.CameraGpuIsolationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabAnalysisRhiTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_ANALYSIS_RHI"))!=TEXT("1")) {AddInfo(TEXT("SKIP: set MA0T10_SLAB_ANALYSIS_RHI=1 to validate actual Camera/GPU pixels."));return true;}
	if(GUsingNullRHI||!FApp::CanEverRender()) {AddError(TEXT("Requested Slab analysis RHI validation requires real rendering."));return false;}
	ADD_LATENT_AUTOMATION_COMMAND(FSlabAnalysisRhiCommand(this));return true;
}
#endif
