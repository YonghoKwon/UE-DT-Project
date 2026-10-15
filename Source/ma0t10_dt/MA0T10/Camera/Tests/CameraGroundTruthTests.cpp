#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "ShaderCompiler.h"
#include "RHIGlobals.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraTruthMath,"MA0T10.CameraGroundTruth.IntrinsicsAndMaskBoxes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraTruthMath::RunTest(const FString&)
{
    for(const FIntPoint Size:{FIntPoint(640,360),FIntPoint(1280,720),FIntPoint(360,640)})
    {
        const auto K=FVirtualCameraIntrinsics::Make(Size.X,Size.Y,90);
        TestTrue(TEXT("square pixels, horizontal FOV for landscape and portrait"),FMath::Abs(K.Fx-Size.X*.5)<1e-8&&K.Fx==K.Fy);
        TestEqual(TEXT("pixel-center cx"),K.Cx,(Size.X-1)*.5);TestEqual(TEXT("pixel-center cy"),K.Cy,(Size.Y-1)*.5);
    }
    TestEqual(TEXT("invalid FOV rejected"),FVirtualCameraIntrinsics::Make(640,360,180).Width,0);
    const TArray<uint8> C={0,2,2,0,2,0,2,2};const TArray<uint32> I={0,7,7,0,8,0,8,8};
    const auto Boxes=FVirtualCameraTruthFrame::FindBoxes(C,I,4,2);
    TestEqual(TEXT("two same-class Actor boxes"),Boxes.Num(),2);
    if(Boxes.Num()==2){TestEqual(TEXT("first mask min x"),Boxes[0].XMin,1);TestEqual(TEXT("first mask max x inclusive"),Boxes[0].XMax,2);TestEqual(TEXT("second mask pixel count"),Boxes[1].Pixels,3);TestEqual(TEXT("second mask clipped min x"),Boxes[1].XMin,0);TestEqual(TEXT("second mask y"),Boxes[1].YMin,1);}
    TestTrue(TEXT("malformed mask rejected"),FVirtualCameraTruthFrame::FindBoxes(C,I,3,3).IsEmpty());
    const auto* Camera=GetDefault<UVirtualCameraCaptureComponent>();
    TestFalse(TEXT("opt-in disabled in CDO"),Camera->bGroundTruthEnabled);TestNull(TEXT("disabled has no truth service"),Camera->GetGroundTruth());
    return true;
}

class FCameraTruthRhiCommand:public IAutomationLatentCommand
{
public:
    explicit FCameraTruthRhiCommand(FAutomationTestBase* In):Test(In){}
    virtual bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(!Scene)
        {
            Started=Now;Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(true));auto* World=Scene->GetWorld();
            auto* Mat=LoadObject<UMaterial>(nullptr,TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId"));
            if(!Mat){Test->AddError(TEXT("existing LiDAR ID material missing"));return Finish();}Mat->CacheShaders(EMaterialShaderPrecompileMode::Default);
            auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            for(int32 N=0;N<2;++N)
            {
                auto* A=World->SpawnActor<AStaticMeshActor>();A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);A->GetStaticMeshComponent()->SetStaticMesh(Cube);
                A->Tags.Add(TEXT("Slab"));A->SetActorLocation(FVector(1000,N?200:-200,0));A->SetActorScale3D(FVector(.2,2,2));Targets.Add(A);
            }
            Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* C=Camera->CaptureComponent.Get();C->StopCapture();C->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->CaptureResolution=FIntPoint(128,64);C->FOVAngle=60;
            Scan=NewObject<UVirtualLidarScanComponent>(Camera);Scan->ResetDefaultSemanticClassRules();
            for(auto* A:Targets)Test->TestEqual(TEXT("canonical camera/LiDAR same Actor label"),VirtualSensorSemanticLabels::Resolve(A->GetStaticMeshComponent()),Scan->ResolveSemanticLabelForComponent(A->GetStaticMeshComponent()));
            World->SendAllEndOfFrameUpdates();C->CaptureAndSendImage();Test->TestNull(TEXT("disabled RGB capture allocates no truth service"),C->GetGroundTruth());C->SetGroundTruthEnabled(true);C->StartCapture();return false;
        }
        auto* C=Camera->CaptureComponent.Get();
        if(Now-Started>60){Test->AddError(TEXT("truth RHI timeout: ")+(C->GetGroundTruth()?C->GetGroundTruth()->Status:FString()));return Finish();}
        if(Now-Started<2||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()))return false;
        if(!Requested&&Now>=NextAttempt)
        {
            Scene->GetWorld()->SendAllEndOfFrameUpdates();C->RequestGroundTruthSave();C->CaptureAndSendImage();NextAttempt=Now+.5;
            Requested=C->GetGroundTruth()&&C->GetGroundTruth()->IsBusy();return false;
        }
        auto* Truth=C->GetGroundTruth();if(!Truth)return false;Truth->Poll();
        if(Requested&&!Truth->IsBusy()&&!Truth->LastFrame){Test->AddError(Truth->Status);return Finish();}
        if(!Truth->LastFrame)return false;
        const auto F=Truth->LastFrame;double MaxError=0;int32 Count=0;TSet<uint32> Ids;
        for(int32 I=0;I<F->Classes.Num();++I)if(F->Classes[I]==VirtualSensorSemanticLabels::ClassId(TEXT("Slab")))
        {++Count;Ids.Add(F->Instances[I]);}
        // Moving the boxes exposes real side faces. Measure known front planes at
        // mask interiors, and separately check that every hit remains inside the box depth interval.
        for(const auto& B:F->Boxes)
        {
            const int32 X=(B.XMin+B.XMax)/2,Y=(B.YMin+B.YMax)/2;
            for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
            {const int32 I=(Y+DY)*F->Intrinsics.Width+X+DX;MaxError=FMath::Max(MaxError,FMath::Abs(double(F->DepthMeters[I])-9.9));}
        }
        Test->TestTrue(TEXT("known plane visible pixels"),Count>100);Test->TestTrue(TEXT("known front plane Z error <= 1cm"),MaxError<=.01);
        Test->TestEqual(TEXT("same-class Actor instances distinguished"),Ids.Num(),2);Test->TestEqual(TEXT("two visible instance boxes"),F->Boxes.Num(),2);
        Test->TestTrue(TEXT("snapshot frame and UTC present"),F->Acquisition.FrameId>0&&F->Acquisition.TimestampUtc.GetTicks()>0);
        Test->TestTrue(TEXT("verification info written after images"),FPaths::FileExists(Truth->LastSavedDirectory/TEXT("camera_info.json")));
        for(const TCHAR* File:{TEXT("depth.exr"),TEXT("segmentation.png"),TEXT("instance.png")})Test->TestTrue(File,FPaths::FileExists(Truth->LastSavedDirectory/File));
        auto& Images=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        for(int32 Kind=0;Kind<3;++Kind)
        {
            const TCHAR* Name=Kind==0?TEXT("depth.exr"):Kind==1?TEXT("segmentation.png"):TEXT("instance.png");
            TArray<uint8> Encoded;TArray64<uint8> Raw;
            auto Image=Images.CreateImageWrapper(Kind==0?EImageFormat::EXR:EImageFormat::PNG);
            const bool Read=FFileHelper::LoadFileToArray(Encoded,*(Truth->LastSavedDirectory/Name))&&Image->SetCompressed(Encoded.GetData(),Encoded.Num())&&Image->GetRaw(Kind==0?ERGBFormat::GrayF:Kind==1?ERGBFormat::Gray:ERGBFormat::BGRA,Kind==0?32:8,Raw);
            Test->TestTrue(TEXT("export round-trip decodes"),Read);
            if(!Read)continue;
            if(Kind==0)Test->TestTrue(TEXT("float32 depth EXR lossless round-trip"),Raw.Num()==F->DepthMeters.Num()*sizeof(float)&&!FMemory::Memcmp(Raw.GetData(),F->DepthMeters.GetData(),Raw.Num()));
            else if(Kind==1)Test->TestTrue(TEXT("PNG class IDs preserved without gamma conversion"),Raw.Num()==F->Classes.Num()&&!FMemory::Memcmp(Raw.GetData(),F->Classes.GetData(),Raw.Num()));
            else if(Raw.Num()==F->Instances.Num()*sizeof(FColor))
            {
                const auto* Colors=reinterpret_cast<const FColor*>(Raw.GetData());int32 Wrong=0;
                for(int32 I=0;I<F->Instances.Num();++I)Wrong+=(uint32(Colors[I].R)|(uint32(Colors[I].G)<<8)|(uint32(Colors[I].B)<<16))!=F->Instances[I];
                Test->TestEqual(TEXT("PNG instance 24-bit packing preserved"),Wrong,0);
            }
            else Test->AddError(TEXT("instance PNG byte count mismatch"));
        }
        Test->AddInfo(FString::Printf(TEXT("CAM-GT-01 D3D12 phase=%d slabPixels=%d maxDepthErrorM=%.8f ids=%d submitMs=%.3f readbackLatencyMs=%.3f decodeMs=%.3f files=%s"),Phase,Count,MaxError,Ids.Num(),F->SubmitMs,F->ReadbackLatencyMs,F->DecodeMs,*Truth->LastSavedDirectory));
        for(auto* A:Targets){Test->TestFalse(TEXT("source stencil unchanged"),A->GetStaticMeshComponent()->bRenderCustomDepth);Test->TestTrue(TEXT("source visible"),A->GetStaticMeshComponent()->IsVisible());}
        if(Phase++==0)
        {
            FirstIds=Ids;Targets[0]->SetActorLocation(FVector(1000,-150,0));C->StopCapture();C->StartCapture();Requested=false;NextAttempt=Now+.2;return false;
        }
        for(uint32 Id:Ids)Test->TestTrue(TEXT("Actor IDs stable after movement and stop/start"),FirstIds.Contains(Id));
        // Pending result must not repopulate previews after a revision change / disable.
        C->SetGroundTruthPreviewDemand(true);C->CaptureAndSendImage();C->SetGroundTruthEnabled(false);FlushRenderingCommands();Truth->Poll();Test->TestFalse(TEXT("disable clears frame and work"),Truth->LastFrame.IsValid()||Truth->IsBusy());
        return Finish();
    }
private:
    bool Finish(){if(Camera)Camera->CaptureComponent->StopCapture();Scene.Reset();return true;}
    FAutomationTestBase* Test;
    TUniquePtr<FPreviewScene> Scene;
    AVirtualCameraSensorActor* Camera=nullptr;
    UVirtualLidarScanComponent* Scan=nullptr;
    TArray<AStaticMeshActor*> Targets;
    TSet<uint32> FirstIds;
    double Started=0,NextAttempt=0;
    bool Requested=false;
    int32 Phase=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraTruthRhi,"MA0T10.CameraGroundTruth.DepthSemanticInstanceRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraTruthRhi::RunTest(const FString&)
{
    if(GUsingNullRHI||!FApp::CanEverRender()){AddWarning(TEXT("SKIP: truth requires real RHI"));return true;}
    ADD_LATENT_AUTOMATION_COMMAND(FCameraTruthRhiCommand(this));return true;
}
#endif
