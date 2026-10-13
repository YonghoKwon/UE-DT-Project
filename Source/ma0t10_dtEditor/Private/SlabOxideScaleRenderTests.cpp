#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "MaterialEditingLibrary.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

/** Unsaved in-memory comparison scene. Captures never enter a production sensor path. */
class FSlabOxideRenderCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;UWorld* World=nullptr;TArray<AActor*> Owned;
    TArray<ASlabActor*> Slabs;USceneCaptureComponent2D* Capture=nullptr;
    double Started=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;
    FString Directory() const
    {
        FString Override;
        if(FParse::Value(FCommandLine::Get(),TEXT("SlabOxideRenderOutput="),Override))
            return FPaths::ConvertRelativePathToFull(Override);
        return FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-08/oxide");
    }
public:
    explicit FSlabOxideRenderCommand(FAutomationTestBase* In):Test(In){}
    ~FSlabOxideRenderCommand(){Cleanup();}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>90){Test->AddError(TEXT("oxide render timeout"));Cleanup();return true;}
        if(!World){Create();return false;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
        if(!ReadyAt){Configure();ReadyAt=Now+1;return false;}
        if(Now<ReadyAt)return false;
        World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();
        TArray<FColor> Pixels;FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);
        if(!Test->TestTrue(TEXT("real RHI capture readback"),Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))){Cleanup();return true;}
        const TCHAR* Names[]={TEXT("presets-five-same-scene.png"),TEXT("oxide-close-oblique.png"),TEXT("oxide-close-top.png"),TEXT("oxide-basecolor.png")};
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Capture->TextureTarget->SizeX,Capture->TextureTarget->SizeY,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
        Test->TestTrue(TEXT("render evidence saved"),FFileHelper::SaveArrayToFile(Png,*(Directory()/Names[Phase])));
        if(Phase==3)
        {
            int32 Dark=0,Bright=0;for(const auto& P:Pixels){Dark+=P.R>10&&P.R<75&&P.B>10;Bright+=P.R>95&&P.B>100;}
            Test->TestTrue(TEXT("oxide shows dark scale and bright peeled steel"),Dark>5000&&Bright>500);
            Test->AddInfo(FString::Printf(TEXT("oxide BaseColor dark=%d peeled=%d"),Dark,Bright));
            WriteStats();Cleanup();return true;
        }
        ++Phase;ReadyAt=0;return false;
    }
private:
    void Create()
    {
        IFileManager::Get().MakeDirectory(*Directory(),true);World=FAutomationEditorCommonUtils::CreateNewMap();
        const ESlabSurfacePreset Presets[]={ESlabSurfacePreset::Default,ESlabSurfacePreset::HotRed,ESlabSurfacePreset::OxidationScale,ESlabSurfacePreset::CooledSteel,ESlabSurfacePreset::AnalysisMatte};
        for(int32 I=0;I<5;++I)
        {
            auto* Slab=World->SpawnActor<ASlabActor>();Owned.Add(Slab);Slabs.Add(Slab);
            Slab->InitialDimensions=FVector(750,350,40);Slab->SetDimensionUnit(ESlabInputUnit::Centimeters);
            Slab->SetActorLocation(FVector((I-2)*500,0,20));Slab->SetActorRotation(FRotator(0,90,0));Slab->SetDiagnosticHelpersVisible(false);
            FString Reason;Test->TestTrue(TEXT("all five presets apply with link off"),Slab->ApplySurfacePreset(Presets[I],false,Reason));
        }
        auto* Floor=World->SpawnActor<AStaticMeshActor>();Owned.Add(Floor);
        Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Floor->SetActorLocation(FVector(0,0,-12));Floor->SetActorScale3D(FVector(34,18,.2));
        for(int32 I=0;I<2;++I)
        {
            auto* Light=World->SpawnActor<ADirectionalLight>();Owned.Add(Light);
            Light->SetActorRotation(I?FRotator(-30,140,0):FRotator(-55,-40,0));
            Light->GetLightComponent()->SetIntensity(I?1.5f:4.f);
            Light->GetLightComponent()->SetCastShadows(false);
        }
        auto* Owner=World->SpawnActor<AActor>();Owned.Add(Owner);Capture=NewObject<USceneCaptureComponent2D>(Owner);Owner->SetRootComponent(Capture);
        Capture->bCaptureEveryFrame=Capture->bCaptureOnMovement=false;Capture->ProjectionType=ECameraProjectionMode::Orthographic;
        Capture->TextureTarget=NewObject<UTextureRenderTarget2D>(Capture);Capture->TextureTarget->ClearColor=FLinearColor(.015,.018,.024,1);
        Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
        Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=0;
        Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
        Capture->ShowFlags.SetAntiAliasing(false);Capture->ShowFlags.SetTemporalAA(false);Capture->ShowFlags.SetMotionBlur(false);Capture->ShowFlags.SetEyeAdaptation(false);
        Capture->ShowFlags.SetScreenSpaceReflections(false);Capture->ShowFlags.SetLumenReflections(false);Capture->ShowFlags.SetLumenGlobalIllumination(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetAtmosphere(false);
        Capture->RegisterComponent();
        for(const TCHAR* Path:{TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain"),TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfaceOxideScale.M_SlabSurfaceOxideScale")})
            if(auto* M=LoadObject<UMaterial>(nullptr,Path))M->CacheShaders(EMaterialShaderPrecompileMode::Default);
    }
    void Configure()
    {
        const bool Comparison=Phase==0;
        Capture->CaptureSource=Phase==3?ESceneCaptureSource::SCS_BaseColor:ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->ShowFlags.SetPostProcessing(Phase!=3);Capture->ShowFlags.SetTonemapper(Phase!=3);
        Capture->OrthoWidth=Comparison?2800:Phase==1?1000:1150;
        Capture->SetWorldLocation(Comparison?FVector(0,1800,2500):Phase==1?FVector(0,650,750):FVector(0,0,1200));
        Capture->SetWorldRotation(Comparison?FRotator(-54,-90,0):Phase==1?FRotator(-48,-90,0):FRotator(-90,-90,0));
        Capture->HiddenActors.Reset();if(!Comparison)for(int32 I=0;I<5;++I)if(I!=2)Capture->HiddenActors.Add(Slabs[I]);
        Capture->TextureTarget->InitCustomFormat(Comparison?2560:1200,Comparison?1440:900,PF_B8G8R8A8,false);
        Capture->TextureTarget->UpdateResourceImmediate(true);
    }
    void WriteStats()
    {
        auto Report=MakeShared<FJsonObject>();
        for(const TCHAR* Name:{TEXT("M_SlabSurfacePlain"),TEXT("M_SlabSurfaceOxideScale")})
        {
            const FString Path=FString(TEXT("/Game/MA0T10/Slab/Materials/"))+Name+TEXT(".")+Name;
            auto* Mat=LoadObject<UMaterial>(nullptr,*Path);const auto Stats=UMaterialEditingLibrary::GetStatistics(Mat);
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("pixel_instructions"),Stats.NumPixelShaderInstructions);
            Row->SetNumberField(TEXT("vertex_instructions"),Stats.NumVertexShaderInstructions);Row->SetNumberField(TEXT("pixel_texture_samples"),Stats.NumPixelTextureSamples);
            Row->SetNumberField(TEXT("samplers"),Stats.NumSamplers);Row->SetNumberField(TEXT("material_textures"),UMaterialEditingLibrary::GetUsedTextures(Mat).Num());Report->SetObjectField(Name,Row);
            Test->TestTrue(TEXT("material has real compiled shader statistics"),Stats.NumPixelShaderInstructions>0);
        }
        Report->SetStringField(TEXT("scope"),TEXT("UE5.3 D3D12 SM6 MaterialEditingLibrary most-expensive cached shader; same in-memory scene/time/lighting; order default/hot/oxide/cooled/matte; no map or existing asset saved"));
        Report->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        Test->TestTrue(TEXT("shader statistics saved"),FFileHelper::SaveStringToFile(Json,*(Directory()/TEXT("material-statistics.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    }
    void Cleanup(){if(Capture){Capture->DestroyComponent();Capture=nullptr;}for(auto* A:Owned)if(IsValid(A))A->Destroy();Owned.Reset();World=nullptr;}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabOxideRenderTest,"MA0T10.UXR08.OxideFivePresetRenderAndShaderCost",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabOxideRenderTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP actual RHI required for oxide renders and shader cost"));return true;}
    ADD_LATENT_AUTOMATION_COMMAND(FSlabOxideRenderCommand(this));return true;
}
#endif
