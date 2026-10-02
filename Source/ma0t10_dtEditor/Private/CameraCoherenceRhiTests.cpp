#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FCameraMarkerCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Start=FPlatformTime::Seconds();
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    TWeakObjectPtr<UMaterialInstanceDynamic> Material;
    TMap<int64,FVector> Poses;
    int64 Last=0;
    int32 Checked=0;
    int32 Step=0;
    double ReadyAt=0;
public:
    explicit FCameraMarkerCheck(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>45){Test->AddError(TEXT("Camera marker RHI timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||FPlatformTime::Seconds()-Start<3)return false;
        if(!Camera.IsValid())
        {
            for(TActorIterator<AVirtualSensorCoordinator> It(W);It;++It)It->StopAllSensors();
            auto* Target=W->SpawnActor<AStaticMeshActor>(FVector(400,0,100),FRotator::ZeroRotator);
            Target->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Target->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Target->SetActorScale3D(FVector(1,10,10));
            auto* Base=NewObject<UMaterial>();Base->SetShadingModel(MSM_Unlit);
            auto* Color=NewObject<UMaterialExpressionVectorParameter>(Base);Color->ParameterName=TEXT("Marker");Color->DefaultValue=FLinearColor::Red;
            Base->GetExpressionCollection().AddExpression(Color);Base->GetEditorOnlyData()->EmissiveColor.Expression=Color;Base->PostEditChange();
            FAssetCompilingManager::Get().FinishAllCompilation();
            Material=UMaterialInstanceDynamic::Create(Base,Target);Target->GetStaticMeshComponent()->SetMaterial(0,Material.Get());
            Camera=W->SpawnActor<AVirtualCameraSensorActor>(FVector(0,0,100),FRotator::ZeroRotator);
            auto* C=Camera->CaptureComponent.Get();
            C->CaptureMode=EVirtualCameraCaptureMode::Payload;C->CaptureResolution=FIntPoint(1280,720);C->CaptureInterval=1.0f/30;
            C->OutputMode=EVirtualCameraOutputMode::None;C->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            C->ShowOnlyActorComponents(Target);C->PostProcessSettings.bOverride_AutoExposureMinBrightness=true;C->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true;
            C->PostProcessSettings.AutoExposureMinBrightness=1;C->PostProcessSettings.AutoExposureMaxBrightness=1;
            C->EncodeDelayForTests=0.04f;
            C->BeforeSceneCaptureForTests=[this](int64 Id)
            {
                Material->SetVectorParameterValue(TEXT("Marker"),Id%2?FLinearColor::Red:FLinearColor::Blue);
                Poses.Add(Id,Camera->CaptureComponent->GetComponentLocation());
            };
            C->StartCapture();ReadyAt=FPlatformTime::Seconds()+3;return false;
        }
        auto* C=Camera->CaptureComponent.Get();
        C->SetWorldLocation(FVector(Checked%4,0,100));
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        const auto& Snapshot=C->GetLastCompletedAcquisition();
        if(Snapshot.FrameId>Last&&C->GetLastJpegSnapshot().IsValid())
        {
            Last=Snapshot.FrameId;
            if(const auto* Pose=Poses.Find(Last))Test->TestTrue(TEXT("RHI JPEG uses captured pose"),Snapshot.Location.Equals(*Pose,0.001));
            auto& Module=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));auto Wrapper=Module.CreateImageWrapper(EImageFormat::JPEG);
            const auto Bytes=C->GetLastJpegSnapshot();TArray64<uint8> Raw;
            if(!Wrapper->SetCompressed(Bytes->GetData(),Bytes->Num())||!Wrapper->GetRaw(ERGBFormat::BGRA,8,Raw)){Test->AddError(TEXT("Marker JPEG decode failed"));return true;}
            const int64 Offset=(int64(Snapshot.Height/2)*Snapshot.Width+Snapshot.Width/2)*4;
            const bool Red=Raw[Offset+2]>Raw[Offset]+40,Blue=Raw[Offset]>Raw[Offset+2]+40;
            if(Checked<3)
            {
                Test->AddInfo(FString::Printf(TEXT("Marker frame=%lld RGB=%d,%d,%d"),Last,Raw[Offset+2],Raw[Offset+1],Raw[Offset]));
                TArray<uint8> Copy;Copy.Append(Bytes->GetData(),Bytes->Num());
                FFileHelper::SaveArrayToFile(Copy,*(FPaths::ProjectSavedDir()/TEXT("Reports/DTCoreSync")/FString::Printf(TEXT("marker-%lld.jpg"),Last)));
            }
            Test->TestTrue(TEXT("actual JPEG marker agrees with acquisition FrameId"),Last%2?Red:Blue);
            Test->TestEqual(TEXT("acquisition UTC survives RHI encode"),C->GetRuntimeStatus().LastUpdateUtc,Snapshot.TimestampUtc);
            ++Checked;
        }
        if(Checked>=20&&Step==0){C->ForceReadbackSaturationForTests=true;Step=1;Start=FPlatformTime::Seconds();return false;}
        if(Step==1&&FPlatformTime::Seconds()-Start>0.3)
        {
            Test->TestTrue(TEXT("slot saturation is explicit failure"),C->GetRuntimeStatus().QueueOverflowCount>0);
            C->ForceReadbackSaturationForTests=false;Step=2;Checked=0;return false;
        }
        if(Step==2&&Checked>=5){C->StopCapture();C->BeforeSceneCaptureForTests=nullptr;Camera->Destroy();return true;}
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraCoherenceRhiTest,"MA0T10.SensorFiles.CameraMarkerRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraCoherenceRhiTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_CAMERA_MARKER_RHI"))!=TEXT("1")){AddInfo(TEXT("SKIP: camera marker requires D3D12 and explicit enable"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FCameraMarkerCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
