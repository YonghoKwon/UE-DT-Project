#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "Async/Async.h"
#include "Containers/Queue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "CameraFrameMarker.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FMarkerExpected
{
    uint64 Token=0;int32 Revision=0;
    FTransform Pose;FDateTime EarliestUtc;
    FVirtualSlabFrameContext Slab;
};
struct FMarkerJob
{
    FVirtualCameraPayloadSnapshot Meta;FVirtualSlabFrameContext Slab;FMarkerExpected Expected;
    TSharedPtr<const TArray64<uint8>,ESPMode::ThreadSafe> Jpeg;
};
struct FMarkerResult {int64 Frame=0;bool Valid=false;FString Error;};
struct FMarkerState
{
    TQueue<FMarkerResult,EQueueMode::Mpsc> Results;
    TAtomic<int32> Active{0};TAtomic<bool> Alive{true};
    TAtomic<int32> SavedFailures{0};
};
bool DecodeMatches(const FMarkerJob& J,FString& Error)
{
    auto& Module=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    auto Decoder=Module.CreateImageWrapper(EImageFormat::JPEG);TArray64<uint8> Pixels;uint64 Token=0;
    if(!J.Jpeg.IsValid()||!Decoder->SetCompressed(J.Jpeg->GetData(),J.Jpeg->Num())||!Decoder->GetRaw(ERGBFormat::BGRA,8,Pixels)) {Error=TEXT("JPEG decode failed");return false;}
    const bool CrcValid=CameraFrameMarker::Decode(Pixels,J.Meta.Width,J.Meta.Height,J.Expected.Pose,J.Meta.HorizontalFov,Token);
    if(!CrcValid||Token!=J.Expected.Token){Error=FString::Printf(TEXT("Unique pixel token/CRC mismatch got=%llu wanted=%llu crc=%d"),Token,J.Expected.Token,CrcValid);return false;}
    if(J.Meta.AcquisitionRevision!=J.Expected.Revision||!J.Meta.Location.Equals(J.Expected.Pose.GetLocation(),.001)||!J.Meta.Rotation.Equals(J.Expected.Pose.Rotator(),.001)){Error=TEXT("Acquisition revision/pose mismatch");return false;}
    if(J.Meta.TimestampUtc<J.Expected.EarliestUtc||(J.Meta.TimestampUtc-J.Expected.EarliestUtc).GetTotalMilliseconds()>50){Error=TEXT("UTC is not at the independent capture boundary");return false;}
    if(J.Slab.RunId!=J.Expected.Slab.RunId||J.Slab.MtlNo!=J.Expected.Slab.MtlNo||J.Slab.SlabFrameNo!=J.Expected.Slab.SlabFrameNo){Error=TEXT("Slab context mismatch");return false;}
    return true;
}
class FCameraMarkerCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    TWeakObjectPtr<AActor> Marker;
    TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Cells;
    TWeakObjectPtr<UVirtualSensorSlabContextSubsystem> Session;
    FString Run;
    TMap<int64,FMarkerExpected> Expected;
    TArray<FMarkerJob> Jobs;
    TSharedRef<FMarkerState,ESPMode::ThreadSafe> State=MakeShared<FMarkerState,ESPMode::ThreadSafe>();
    TOptional<FMarkerJob> Previous;
    double Start=FPlatformTime::Seconds(),Ready=0,StageAt=0;
    int32 Stage=0,Completed=0,Checked=0,BeforeStop=0,OverflowBefore=0;
    bool NegativeChecked=false;
    void Pump()
    {
        while(!Jobs.IsEmpty()&&State->Active.Load()<2)
        {
            FMarkerJob J=MoveTemp(Jobs[0]);Jobs.RemoveAt(0,1,false);++State->Active;
            Async(EAsyncExecution::ThreadPool,[S=State,J=MoveTemp(J)](){FMarkerResult R;R.Frame=J.Meta.FrameId;R.Valid=DecodeMatches(J,R.Error);if(!R.Valid&&++S->SavedFailures<=3)FFileHelper::SaveArrayToFile(*J.Jpeg,*(FPaths::ProjectSavedDir()/TEXT("Reports/DTCoreFinalAcceptance")/FString::Printf(TEXT("marker-failure-%lld.jpg"),R.Frame)));S->Results.Enqueue(MoveTemp(R));--S->Active;});
        }
    }
public:
    explicit FCameraMarkerCheck(FAutomationTestBase* In):Test(In){}
    ~FCameraMarkerCheck()
    {
        State->Alive.Store(false);
        if(Camera.IsValid()){Camera->CaptureComponent->BeforeSceneCaptureForTests=nullptr;Camera->CaptureComponent->OnScheduledFrameForTests=nullptr;Camera->StopSensor();Camera->Destroy();}
        if(Marker.IsValid())Marker->Destroy();
        if(Session.IsValid()&&!Run.IsEmpty())Session->EndSlabSensorSession(Run,true);
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>100){Test->AddError(TEXT("Unique camera marker timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||FPlatformTime::Seconds()-Start<3)return false;
        if(!Camera.IsValid()&&Stage==0)
        {
            AVirtualSensorCoordinator* Coordinator=nullptr;for(TActorIterator<AVirtualSensorCoordinator> It(W);It;++It){Coordinator=*It;It->StopAllSensors();}
            Marker=W->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(Marker.Get());Marker->SetRootComponent(Root);Root->RegisterComponent();
            auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            auto* Material=NewObject<UMaterial>();Material->SetShadingModel(MSM_Unlit);
            auto* Parameter=NewObject<UMaterialExpressionScalarParameter>(Material);Parameter->ParameterName=TEXT("Bit");Parameter->DefaultValue=0;
            Material->GetExpressionCollection().AddExpression(Parameter);Material->GetEditorOnlyData()->EmissiveColor.Expression=Parameter;Material->PostEditChange();
            for(int32 Index=0;Index<CameraFrameMarker::Count;++Index)
            {
                auto* I=NewObject<UStaticMeshComponent>(Marker.Get());I->SetupAttachment(Root);I->SetStaticMesh(Mesh);
                auto* Dynamic=UMaterialInstanceDynamic::Create(Material,I);I->SetMaterial(0,Dynamic);Cells.Add(Dynamic);
                I->SetRelativeLocation(CameraFrameMarker::CellLocation(Index));I->SetRelativeScale3D(FVector(.05,.22,.22));
                I->SetCollisionEnabled(ECollisionEnabled::NoCollision);I->CastShadow=false;I->RegisterComponent();Marker->AddInstanceComponent(I);
            }
            FAssetCompilingManager::Get().FinishAllCompilation();FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
            Camera=W->SpawnActor<AVirtualCameraSensorActor>(FVector(0,0,100),FRotator::ZeroRotator);auto* C=Camera->CaptureComponent.Get();C->StopCapture();
            C->SensorId=TEXT("CAMERA-COHERENCE-TEST");Coordinator->RegisterSensorActor(Camera.Get());
            C->CaptureMode=EVirtualCameraCaptureMode::Payload;C->CaptureResolution=FIntPoint(1280,720);C->CaptureInterval=1.0f/30;C->FOVAngle=87;C->OutputMode=EVirtualCameraOutputMode::None;
            C->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;C->ShowOnlyActorComponents(Marker.Get());
            C->PostProcessSettings.bOverride_AutoExposureMinBrightness=true;C->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true;
            C->PostProcessSettings.AutoExposureMinBrightness=1;C->PostProcessSettings.AutoExposureMaxBrightness=1;
            Session=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();Run=Session->BeginReplaySensorSession(FGuid::NewGuid().ToString(),{C->SensorId},FGuid::NewGuid().ToString(),false);
            if(Run.IsEmpty()){Test->AddError(TEXT("Marker observation session failed"));return true;}
            Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),0,0);
            C->EncodeDelayForTests=.04f;
            C->BeforeSceneCaptureForTests=[this](int64 Id)
            {
                auto* Capture=Camera->CaptureComponent.Get();FMarkerExpected E;
                E.Revision=Capture->GetAcquisitionRevisionForTests();E.Token=(uint64(E.Revision+1)<<48)|uint64(Id);
                E.Pose=Capture->GetComponentTransform();E.Slab=Session->GetSlabSensorSessionStatus().CurrentSlab;
                for(int32 I=0;I<CameraFrameMarker::Count;++I)Cells[I]->SetScalarParameterValue(TEXT("Bit"),CameraFrameMarker::Cell(E.Token,I)?1.0f:0.0f);
                E.EarliestUtc=FDateTime::UtcNow();Expected.Add(Id,E);
            };
            C->OnScheduledFrameForTests=[this](const FVirtualCameraPayloadSnapshot& M,const FVirtualSlabFrameContext& Slab,TSharedPtr<const TArray64<uint8>,ESPMode::ThreadSafe> Bytes)
            {
                if(FPlatformTime::Seconds()<Ready)return;
                const auto* E=Expected.Find(M.FrameId);if(!E){Test->AddError(TEXT("Completion has no independent capture oracle"));return;}
                FMarkerJob J;J.Meta=M;J.Slab=Slab;J.Expected=*E;J.Jpeg=MoveTemp(Bytes);++Completed;
                if(Jobs.Num()>=8){Test->AddError(TEXT("Bounded verification decoder queue overflow"));return;}
                if(!NegativeChecked&&Previous.IsSet())
                {
                    FMarkerJob Wrong=J;Wrong.Jpeg=Previous->Jpeg;FString Error;
                    Test->TestFalse(TEXT("stale image with new metadata is rejected"),DecodeMatches(Wrong,Error));NegativeChecked=true;
                }
                Previous=J;Jobs.Add(MoveTemp(J));
            };
            C->StartCapture();Ready=FPlatformTime::Seconds()+3;StageAt=Ready;Stage=1;return false;
        }
        auto* C=Camera.IsValid()?Camera->CaptureComponent.Get():nullptr;
        Pump();FMarkerResult R;while(State->Results.Dequeue(R)){++Checked;if(!R.Valid)Test->AddError(FString::Printf(TEXT("Camera frame %lld: %s"),R.Frame,*R.Error));}
        if(Stage==1)
        {
            const double Age=FMath::Max(0.0,FPlatformTime::Seconds()-Ready);const int64 SlabFrame=FMath::Min<int64>(599,FMath::FloorToInt(Age*20));
            Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),SlabFrame,SlabFrame*.05);
            C->SetWorldLocationAndRotation(FVector(FMath::Sin(Age)*3,0,100),FRotator(0,FMath::Sin(Age)*1.5,0));
            if(Age<30)return false;
            Test->TestTrue(TEXT("30 seconds checks every completed frame"),Completed>=870);OverflowBefore=C->GetRuntimeStatus().QueueOverflowCount;
            C->ForceReadbackSaturationForTests=true;Stage=2;StageAt=FPlatformTime::Seconds();return false;
        }
        if(Stage==2&&FPlatformTime::Seconds()-StageAt>.3)
        {
            Test->TestTrue(TEXT("saturation has explicit failures"),C->GetRuntimeStatus().QueueOverflowCount>OverflowBefore);C->ForceReadbackSaturationForTests=false;
            C->StopCapture();C->CaptureResolution=FIntPoint(640,360);C->StartCapture();Stage=3;StageAt=FPlatformTime::Seconds();return false;
        }
        if(Stage==3&&FPlatformTime::Seconds()-StageAt>2)
        {
            C->StopCapture();BeforeStop=Completed;Stage=4;StageAt=FPlatformTime::Seconds();return false;
        }
        if(Stage==4&&FPlatformTime::Seconds()-StageAt>.3)
        {
            Test->TestEqual(TEXT("no stale output after Stop"),Completed,BeforeStop);C->StartCapture();Stage=5;StageAt=FPlatformTime::Seconds();return false;
        }
        if(Stage==5&&FPlatformTime::Seconds()-StageAt>1)
        {
            C->OnScheduledFrameForTests=nullptr;C->BeforeSceneCaptureForTests=nullptr;Camera->Destroy();BeforeStop=Completed;Stage=6;StageAt=FPlatformTime::Seconds();return false;
        }
        if(Stage==6&&FPlatformTime::Seconds()-StageAt>.3&&Jobs.IsEmpty()&&State->Active.Load()==0)
        {
            Test->TestEqual(TEXT("every completed JPEG independently decoded"),Checked,Completed);Test->TestTrue(TEXT("negative control ran"),NegativeChecked);
            Test->TestEqual(TEXT("actor deletion cannot apply late output"),Completed,BeforeStop);
            Test->AddInfo(FString::Printf(TEXT("Unique marker checked=%d completed=%d bounded workers=2"),Checked,Completed));return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraCoherenceRhiTest,"MA0T10.SensorFiles.CameraMarkerRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraCoherenceRhiTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_CAMERA_MARKER_RHI"))!=TEXT("1")){AddInfo(TEXT("SKIP: camera marker requires D3D12 and explicit enable"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCameraMarkerCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
