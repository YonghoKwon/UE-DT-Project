#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "AssetCompilingManager.h"
#include "DrawDebugHelpers.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"

class FDepthPreviewIsolationCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;UWorld* World;AVirtualCameraSensorActor* Camera;AVirtualLidarSensorActor* Markers;AVirtualLidarSensorActor* DepthSensor;AStaticMeshActor* Wall;UInstancedStaticMeshComponent* Preview;
    FVirtualLidarDepthAcquisitionFrame Off;int32 Stage=0;double Started=FPlatformTime::Seconds();
    void Cleanup(){DepthSensor->GpuDepthProjectionComponent->CancelAcquisition();Camera->Destroy();Markers->Destroy();DepthSensor->Destroy();Wall->Destroy();}
    bool Begin(int64 Id){World->SendAllEndOfFrameUpdates();FVirtualLidarDepthAcquisitionRequest Q;Q.FrameId=Id;Q.AcquisitionTransform=Camera->CaptureComponent->GetComponentTransform();Q.HorizontalSamples=64;Q.VerticalChannels=16;Q.HorizontalFovDegrees=90;Q.MaxDistanceCm=1000;return DepthSensor->GpuDepthProjectionComponent->BeginAcquisition(Q);}
public:
    FDepthPreviewIsolationCheck(FAutomationTestBase* T,UWorld* W,AVirtualCameraSensorActor* C,AVirtualLidarSensorActor* M,AStaticMeshActor* A,UInstancedStaticMeshComponent* P):Test(T),World(W),Camera(C),Markers(M),Wall(A),Preview(P)
    {DepthSensor=World->SpawnActor<AVirtualLidarSensorActor>();DepthSensor->StopSensor();}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>20){Test->AddError(TEXT("GPU capture isolation fixture timeout"));Cleanup();return true;}
        if(Stage==0){Preview->SetVisibility(false);if(!Begin(1)){Test->AddError(TEXT("GPU fixture failed to begin"));Cleanup();return true;}Stage=1;return false;}
        FVirtualLidarDepthAcquisitionFrame Frame;const auto Poll=DepthSensor->GpuDepthProjectionComponent->PollAcquisition(Frame);
        if(Poll==EVirtualSensorBackendPollResult::Pending)return false;
        if(Poll!=EVirtualSensorBackendPollResult::Completed){Test->AddError(DepthSensor->GpuDepthProjectionComponent->GetBackendStatusMessage());Cleanup();return true;}
        if(Stage==1){Off=MoveTemp(Frame);Preview->SetVisibility(true);Begin(2);Stage=2;return false;}
        const int32 Center=(Off.CaptureHeight/2)*Off.CaptureWidth+Off.CaptureWidth/2;
        if(!Off.ForwardDepthCentimeters.IsValidIndex(Center)||!Frame.ForwardDepthCentimeters.IsValidIndex(Center)){Test->AddError(TEXT("GPU fixture has no center sample"));Cleanup();return true;}
        if(Stage==2){
            Test->AddInfo(FString::Printf(TEXT("GPU isolation centre off=%.3f on=%.3f pose=%s"),Off.ForwardDepthCentimeters[Center],Frame.ForwardDepthCentimeters[Center],*Off.Request.AcquisitionTransform.GetLocation().ToString()));
            Test->TestTrue(TEXT("GPU fixture measures the actual wall"),Off.ForwardDepthCentimeters[Center]>500&&Off.ForwardDepthCentimeters[Center]<1000);
            Test->TestEqual(TEXT("GPU sample count unaffected by markers"),Off.ForwardDepthCentimeters.Num(),Frame.ForwardDepthCentimeters.Num());
            float Error=0;for(int32 I=0;I<FMath::Min(Off.ForwardDepthCentimeters.Num(),Frame.ForwardDepthCentimeters.Num());++I)Error=FMath::Max(Error,FMath::Abs(Off.ForwardDepthCentimeters[I]-Frame.ForwardDepthCentimeters[I]));
            Test->TestTrue(TEXT("GPU depth unchanged by markers"),Error<.01f);Preview->SetHiddenInSceneCapture(false);Begin(3);Stage=3;return false;}
        Test->AddInfo(FString::Printf(TEXT("GPU isolation negative centre=%.3f"),Frame.ForwardDepthCentimeters[Center]));
        Test->TestTrue(TEXT("GPU negative control detects marker contamination"),Off.ForwardDepthCentimeters[Center]-Frame.ForwardDepthCentimeters[Center]>100);
        Preview->SetHiddenInSceneCapture(true);Cleanup();return true;
    }
};

static bool RunCapturePreviewFixture(FAutomationTestBase* Test,UWorld* W)
{
    auto TestEqual=[&](const TCHAR* Name,const auto& A,const auto& B){return Test->TestEqual(Name,A,B);};
    auto TestTrue=[&](const TCHAR* Name,bool Value){return Test->TestTrue(Name,Value);};
    auto TestNotNull=[&](const TCHAR* Name,const auto* Value){return Test->TestNotNull(Name,Value);};
    auto AddInfo=[&](const FString& Message){Test->AddInfo(Message);};
    for(TActorIterator<AVirtualSensorActorBase> It(W);It;++It)It->StopSensor();
    auto Material=[](FLinearColor Color){auto* M=NewObject<UMaterial>();M->SetShadingModel(MSM_Unlit);M->bUsedWithInstancedStaticMeshes=true;
        auto* E=NewObject<UMaterialExpressionConstant3Vector>(M);E->Constant=Color;M->GetExpressionCollection().AddExpression(E);M->GetEditorOnlyData()->EmissiveColor.Expression=E;M->PostEditChange();return M;};
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Wall=W->SpawnActor<AStaticMeshActor>();Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);Wall->GetStaticMeshComponent()->SetMaterial(0,Material(FLinearColor(.15,.15,.15)));
    Wall->SetActorLocation(FVector(100800,0,100));Wall->SetActorScale3D(FVector(.1,30,30));
    auto* Camera=W->SpawnActor<AVirtualCameraSensorActor>();auto* C=Camera->CaptureComponent.Get();C->StopCapture();C->SetWorldLocationAndRotation(FVector(100000,0,100),FRotator::ZeroRotator);
    C->CaptureResolution=FIntPoint(128,128);C->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->OutputMode=EVirtualCameraOutputMode::None;C->FOVAngle=90;
    // This editor-world fixture has no BeginPlay or exposure warmup. Use direct
    // scene colour so the exclusion test cannot be hidden by temporal exposure.
    C->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;C->ShowFlags.SetEyeAdaptation(false);
    auto* Lidar=W->SpawnActor<AVirtualLidarSensorActor>();Lidar->StopSensor();Lidar->ScanComponent->SetPointCloudPreviewEnabled(true);
    TArray<UInstancedStaticMeshComponent*> Components;Lidar->GetComponents(Components);UInstancedStaticMeshComponent* Preview=nullptr;
    for(auto* P:Components)if(P->GetName()==TEXT("VirtualLidarPointCloudPreview"))Preview=P;
    if(!TestNotNull(TEXT("owned CPU diagnostic fixture exists"),Preview))return false;
    Preview->SetMaterial(0,Material(FLinearColor::Red));Preview->AddInstance(FTransform(FRotator::ZeroRotator,FVector(100300,0,100),FVector(2)),true);
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto Pixels=[&](){W->SendAllEndOfFrameUpdates();C->CaptureAndSendImage();FlushRenderingCommands();TArray<FColor> P;C->CameraRenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(P);return P;};
    Preview->SetVisibility(false);const auto Base=Pixels();
    TestEqual(TEXT("Camera fixture has actual pixels"),Base.Num(),128*128);
    Preview->SetVisibility(true);const auto Excluded=Pixels();
    auto Difference=[](const TArray<FColor>& A,const TArray<FColor>& B){int32 N=0;for(int32 I=0;I<FMath::Min(A.Num(),B.Num());++I)N+=(FMath::Abs(int(A[I].R)-int(B[I].R))+FMath::Abs(int(A[I].G)-int(B[I].G))+FMath::Abs(int(A[I].B)-int(B[I].B)))>6;return N;};
    TestEqual(TEXT("preview ON/OFF leaves camera pixels unchanged"),Difference(Base,Excluded),0);
    Preview->SetHiddenInSceneCapture(false);const auto Negative=Pixels();
    if(Base.Num()&&Negative.Num())AddInfo(FString::Printf(TEXT("Camera isolation centre base=%s negative=%s changed=%d"),*Base[Base.Num()/2+64].ToString(),*Negative[Negative.Num()/2+64].ToString(),Difference(Base,Negative)));
    TestTrue(TEXT("negative control marker would measurably contaminate capture"),Difference(Base,Negative)>100);
    Preview->SetHiddenInSceneCapture(true);DrawDebugLine(W,FVector(100200,-150,100),FVector(100200,150,100),FColor::Green,true,1,0,25);
    const auto WithDebug=Pixels();TestEqual(TEXT("gizmo/debug line batchers do not enter camera measurements"),Difference(Base,WithDebug),0);
    ADD_LATENT_AUTOMATION_COMMAND(FDepthPreviewIsolationCheck(Test,W,Camera,Lidar,Wall,Preview));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
class FStartPreviewCaptureFixtureCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();
public:
    explicit FStartPreviewCaptureFixtureCheck(FAutomationTestBase* T):Test(T){}
    bool Update() override{if(FPlatformTime::Seconds()-Started<3)return false;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return RunCapturePreviewFixture(Test,C.World());return false;}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorCapturePreviewIsolationRhiTest,"MA0T10.ResourceLifecycle.PreviewCaptureIsolationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorCapturePreviewIsolationRhiTest::RunTest(const FString&)
{
    if(GUsingNullRHI||!FApp::CanEverRender()){AddInfo(TEXT("SKIP: capture isolation requires actual RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FStartPreviewCaptureFixtureCheck(this));return true;
}
#endif
