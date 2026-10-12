#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/DirectionalLight.h"
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
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"

/** Actual sensor snapshots. Fixed envelope identity isolates bytes from wall-clock/FrameId changes. */
class FSlabSurfaceRhiCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;UWorld* World=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Sensor=nullptr;USceneCaptureComponent2D* Capture=nullptr;AActor* CaptureOwner=nullptr;AActor* Floor=nullptr;ADirectionalLight* Light=nullptr;
    int32 Backend=0,Phase=0;bool Requested=false,Captured=false;int64 PreviousFrame=-1;double Started=FPlatformTime::Seconds(),ReadyAt=0;
    TArray<FColor> Pixels0;TArray<FVirtualLidarPoint> Points0;TArray<uint8> Pcd0;TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();FString Run;
public:
    explicit FSlabSurfaceRhiCommand(FAutomationTestBase* T):Test(T){}
    ~FSlabSurfaceRhiCommand(){Cleanup();}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>100){Test->AddError(FString::Printf(TEXT("Surface sensor regression timeout backend=%d phase=%d"),Backend,Phase));return Finish();}
        if(!World){if(!Create())return Finish();return false;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
        if(!ReadyAt)
        {
            FString Reason;const auto P=Phase<2||Phase==6?ESlabSurfacePreset::Default:Phase<4?ESlabSurfacePreset::HotRed:ESlabSurfacePreset::OxidationScale;
            if(Phase>0)Test->TestTrue(TEXT("explicit surface transition accepted"),Slab->ApplySurfacePreset(P,Phase==3||Phase==5,Reason));
            auto* Scan=Sensor->ScanComponent.Get();Scan->StopScan();Scan->AcquisitionBackend=Backend==0?EVirtualLidarAcquisitionBackend::AccurateCpuTrace:EVirtualLidarAcquisitionBackend::GpuDepthProjection;
            ReadyAt=Now+.8;return false;
        }
        if(Now<ReadyAt)return false;
        if(!Captured)
        {
            World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();
            TArray<FColor> Pixels;FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);
            if(!Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)){Test->AddError(TEXT("Camera readback failed"));return Finish();}
            SavePixels(Pixels);
            if(Phase==0){Pixels0=Pixels;int32 Lit=0;for(const auto& P:Pixels)Lit+=P.R>5||P.G>5||P.B>5;Test->TestTrue(TEXT("Camera renders a physical surface"),Lit>1000);}
            else if(Phase==1||Phase==6)Test->TestTrue(TEXT("default/link-off and restored Camera pixels exactly unchanged"),Pixels==Pixels0);
            else if(Phase==2)Test->TestTrue(TEXT("hot appearance is a visible positive control"),Pixels!=Pixels0);
            else if(Phase==4)Test->TestTrue(TEXT("oxide appearance visibly changes the surface"),Pixels!=Pixels0);
            Captured=true;
        }
        auto* Scan=Sensor->ScanComponent.Get();
        if(!Requested){Scan->StartScan();Scan->RequestImmediateScheduledScan();Scan->PrepareScheduledScan(World->GetTimeSeconds());Requested=true;}
        Scan->ProcessScheduledScanChunk(1024);const auto Frame=Scan->GetLastFrameSnapshot();
        if(!Frame||!Frame->Points||Frame->FrameId==PreviousFrame)return false;
        PreviousFrame=Frame->FrameId;Scan->StopScan();
        Test->TestTrue(TEXT("immutable frame confirms the actual backend"),Frame->AcquisitionBackend==(Backend==0?EVirtualLidarAcquisitionBackend::AccurateCpuTrace:EVirtualLidarAcquisitionBackend::GpuDepthProjection));
        int32 SlabHits=0;double Intensity=0;for(const auto& P:*Frame->Points)if(P.bHit&&P.SemanticLabel==TEXT("Slab")){++SlabHits;Intensity+=P.RawIntensity;}
        Test->TestTrue(TEXT("LiDAR measures the physical Slab"),SlabHits>20);const double Mean=SlabHits?Intensity/SlabHits:0;
        Test->AddInfo(FString::Printf(TEXT("Surface RHI backend=%d phase=%d slabHits=%d meanRawIntensity=%.3f frame=%lld"),Backend,Phase,SlabHits,Mean,Frame->FrameId));
        Report->SetStringField(FString::Printf(TEXT("%s_phase%d_backendStatus"),Backend?TEXT("gpu"):TEXT("cpu"),Phase),Scan->GetRuntimeStatus().AcquisitionBackendMessage);
        Report->SetNumberField(FString::Printf(TEXT("%s_phase%d_intensity"),Backend?TEXT("gpu"):TEXT("cpu"),Phase),Mean);
        FVirtualSensorFrameEnvelope Envelope;Envelope.SensorId=TEXT("surface-fixture");Envelope.FrameId=42;Envelope.TimestampUtc=FDateTime(2026,1,1);Envelope.PointSnapshot=Frame->Points;
        FVirtualSensorStreamConfig Config;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Config.PcdDataMode=EVirtualPcdDataMode::Binary;FString Ext,Error;int32 Count=0;TArray<uint8> Bytes;
        Test->TestTrue(TEXT("actual points serialize as existing binary PCD"),UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Envelope,Config,Ext,Bytes,Count,Error));
        if(Phase==0){Points0=*Frame->Points;Pcd0=Bytes;}
        else if(Phase==1||Phase==2||Phase==4||Phase==6)
        {
            bool Equal=Points0.Num()==Frame->Points->Num();if(Equal)for(int32 I=0;I<Points0.Num();++I)Equal&=Points0[I].RawIntensity==(*Frame->Points)[I].RawIntensity&&Points0[I].NormalizedIntensity==(*Frame->Points)[I].NormalizedIntensity;
            Test->TestTrue(TEXT("link-off intensity exactly unchanged"),Equal);Test->TestTrue(TEXT("link-off PCD bytes exactly unchanged with fixed envelope identity"),Bytes==Pcd0);
        }
        else
        {
            const double Baseline=Report->GetNumberField(FString::Printf(TEXT("%s_phase0_intensity"),Backend?TEXT("gpu"):TEXT("cpu")));
            Test->TestTrue(TEXT("explicit reflectivity linkage changes measured intensity"),Mean>0&&Mean<Baseline*.8);Test->TestTrue(TEXT("linked PCD bytes reflect changed intensity"),Bytes!=Pcd0);
            auto* Results=World->GetSubsystem<USlabRunResultsSubsystem>();FSlabRunDeliverySummary Summary;Results->GetRunResult(Run,Summary);
            TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Results->SerializeSummary(Summary)),Json);
            auto Latest=Json->GetObjectField(TEXT("surfaceAppearance"))->GetObjectField(TEXT("latest"));
            Test->TestTrue(TEXT("result records the optical setting that changed intensity"),Latest->GetBoolField(TEXT("lidarLinked"))&&FMath::IsNearlyEqual(Latest->GetNumberField(TEXT("reflectivity940Nm")),Phase==3?.08:.12,1.e-6));
            Test->TestEqual(TEXT("result records actual linked preset"),Latest->GetStringField(TEXT("preset")),ASlabActor::SurfacePresetId(Phase==3?ESlabSurfacePreset::HotRed:ESlabSurfacePreset::OxidationScale));
            Report->SetObjectField(TEXT("runResult"),Json);
        }
        if(Phase==6)
        {
            if(Backend==1)return Finish();
            Backend=1;Phase=-1;FString Reason;Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,Reason);
        }
        ++Phase;ReadyAt=0;Requested=Captured=false;return false;
    }
private:
    FString Directory() const{const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));return (Root.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-08"):Root)/TEXT("sensor-regression");}
    bool Create()
    {
        World=FAutomationEditorCommonUtils::CreateNewMap();
        if(auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId")))Material->CacheShaders(EMaterialShaderPrecompileMode::Default);
        Light=World->SpawnActor<ADirectionalLight>();Light->SetActorRotation(FRotator(-60,30,0));
        auto* FloorActor=World->SpawnActor<AStaticMeshActor>();Floor=FloorActor;FloorActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);FloorActor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));FloorActor->SetActorLocation(FVector(0,0,-5));FloorActor->SetActorScale3D(FVector(20,20,.1));
        Slab=World->SpawnActor<ASlabActor>();Slab->InitialDimensions=FVector(1000,200,25);Slab->SetDimensionUnit(ESlabInputUnit::Centimeters);Slab->SetActorLocation(FVector(0,0,12.5));Slab->SetDiagnosticHelpersVisible(false);
        auto* Results=World->GetSubsystem<USlabRunResultsSubsystem>();if(!Results){Test->AddError(TEXT("Preview world result subsystem unavailable"));return false;}Results->ReportDirectoryOverrideForTests=Directory()/TEXT("runs");
        Run=FGuid::NewGuid().ToString();Slab->Status.RunUUID=Run;Results->BeginRun(Run,TEXT("surface-fixture"),ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());
        Sensor=World->SpawnActor<AVirtualLidarSensorActor>();Sensor->SetActorLocation(FVector(0,0,1000));Sensor->SetActorRotation(FRotator(-90,0,0));
        auto* Scan=Sensor->ScanComponent.Get();Scan->StopScan();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->HorizontalSamples=120;Scan->VerticalChannels=32;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;Scan->bEnableRangeNoise=false;Scan->bEnableSemanticClassification=true;Scan->ViewMode=EVirtualLidarViewMode::ActorClassColor;Scan->SemanticClassRules.Reset();Scan->SetInteractivePreviewMode(false,true);
        FVirtualLidarSemanticClassRule Rule;Rule.Label=TEXT("Slab");Rule.ActorClassNames.Add(ASlabActor::StaticClass()->GetFName());Scan->SemanticClassRules.Add(Rule);
        CaptureOwner=World->SpawnActor<AActor>();Capture=NewObject<USceneCaptureComponent2D>(CaptureOwner);CaptureOwner->SetRootComponent(Capture);Capture->bCaptureEveryFrame=Capture->bCaptureOnMovement=false;Capture->CaptureSource=ESceneCaptureSource::SCS_BaseColor;Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=1600;
        Capture->ShowFlags.SetPostProcessing(false);Capture->ShowFlags.SetTonemapper(false);Capture->ShowFlags.SetAntiAliasing(false);Capture->ShowFlags.SetTemporalAA(false);Capture->ShowFlags.SetMotionBlur(false);Capture->ShowFlags.SetEyeAdaptation(false);Capture->ShowFlags.SetAmbientOcclusion(false);Capture->ShowFlags.SetDynamicShadows(false);Capture->ShowFlags.SetScreenSpaceReflections(false);Capture->ShowFlags.SetLumenReflections(false);Capture->ShowFlags.SetLumenGlobalIllumination(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetAtmosphere(false);
        Capture->TextureTarget=NewObject<UTextureRenderTarget2D>(Capture);Capture->TextureTarget->ClearColor=FLinearColor::Black;Capture->TextureTarget->InitCustomFormat(512,512,PF_B8G8R8A8,false);Capture->TextureTarget->UpdateResourceImmediate(true);Capture->RegisterComponent();Capture->SetWorldTransform(FTransform(FRotator(-90,0,0),FVector(0,0,1500)));World->SendAllEndOfFrameUpdates();return true;
    }
    void SavePixels(const TArray<FColor>& Pixels)
    {IFileManager::Get().MakeDirectory(*Directory(),true);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(512,512,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);FFileHelper::SaveArrayToFile(Png,*(Directory()/FString::Printf(TEXT("%s-phase%d.png"),Backend?TEXT("gpu"):TEXT("cpu"),Phase)));}
    bool Finish()
    {Report->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());Report->SetStringField(TEXT("scope"),TEXT("D3D12 deterministic BaseColor SceneCapture pixels + actual CPU/GPU LiDAR snapshots; existing semantic capture enabled; fixed envelope identity for binary PCD; no production FinalColor/manual claim"));FString Text;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Directory(),true);FFileHelper::SaveStringToFile(Text,*(Directory()/TEXT("result.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Cleanup();return true;}
    void Cleanup(){if(!World)return;if(Sensor){Sensor->ScanComponent->StopScan();Sensor->GpuDepthProjectionComponent->CancelAcquisition();Sensor->GpuDepthProjectionComponent->UnregisterComponent();}if(Capture){Capture->DestroyComponent();Capture=nullptr;}for(AActor* A:{static_cast<AActor*>(Sensor),static_cast<AActor*>(Slab),Floor,CaptureOwner,static_cast<AActor*>(Light)})if(IsValid(A))A->Destroy();Sensor=nullptr;Slab=nullptr;World=nullptr;}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabSurfaceRhiTest,"MA0T10.UXR08.DefaultCameraIntensityPcdAndLinkedResponseRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabSurfaceRhiTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP actual RHI is required"));return true;}
    ADD_LATENT_AUTOMATION_COMMAND(FSlabSurfaceRhiCommand(this));return true;
}
#endif
