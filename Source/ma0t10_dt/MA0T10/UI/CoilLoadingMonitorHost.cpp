#include "CoilLoadingMonitor.h"
#include "CoilLoadingPlanProjection.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "VirtualSensorUiHostActor.h"
#include "Core/DxWidgetSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "SceneViewExtension.h"
#include "SceneView.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

class FCoilLoadingViewExtension final:public FWorldSceneViewExtension
{
public:
    FCoilLoadingViewExtension(const FAutoRegister& R,UWorld* W):FWorldSceneViewExtension(R,W){}
    FVector4 Rect=FVector4(0,0,1,1);bool bPointOnly=false;
    void SetupViewFamily(FSceneViewFamily&) override{}
    void SetupView(FSceneViewFamily& Family,FSceneView& View) override{if(bPointOnly&&!View.bIsSceneCapture&&!View.bIsReflectionCapture)Family.EngineShowFlags.SetAtmosphere(false);}
    void BeginRenderViewFamily(FSceneViewFamily&) override{}
    void SetupViewProjectionMatrix(FSceneViewProjectionData& Data) override
    {
        // A nonzero native ViewRect origin can crop the final UE5.3 upscale
        // target. Keep the full target and shift only the player projection;
        // opaque native UI masks cover the other regions. No extra capture.
        CoilPlan::FitPlayerProjection(Data.ProjectionMatrix,Data.GetViewRect(),Rect);
    }
};
ACoilLoadingMonitorHost::ACoilLoadingMonitorHost(){PrimaryActorTick.bCanEverTick=false;}
void ACoilLoadingMonitorHost::BeginPlay(){Super::BeginPlay();GetWorldTimerManager().SetTimer(Timer,this,&ACoilLoadingMonitorHost::Refresh,.2f,true);}
void ACoilLoadingMonitorHost::Refresh()
{
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||!Simulation||!Simulation->Lidar)return;
    if(!Monitor)
    {
        bPreviousDebugText=GEngine->bEnableOnScreenDebugMessagesDisplay;GEngine->bEnableOnScreenDebugMessagesDisplay=false;
        PreviousView=PC->GetViewTarget();bPreviousCursor=PC->bShowMouseCursor;PC->bShowMouseCursor=true;FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);PC->SetInputMode(Mode);
        Extension=FSceneViewExtensions::NewExtension<FCoilLoadingViewExtension>(GetWorld());
        FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;Coordinator=GetWorld()->SpawnActor<AVirtualSensorCoordinator>(P);Coordinator->RegisterSensorActor(Simulation->Lidar);
        // Explicitly owned sensor only; use the existing binary PCD/FIFO default.
        if(Coordinator->StreamPublisherComponent)Coordinator->StreamPublisherComponent->StartStream(EVirtualSensorStreamKind::PointCloud,Simulation->Lidar->GetSensorId());
        Monitor=CreateWidget<UCoilLoadingMonitor>(PC,UCoilLoadingMonitor::StaticClass());Monitor->Bind(this,Simulation);Monitor->AddToViewport(20);
        if(auto* D=GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>()){D->RegisterWidget(Monitor);D->RegisterExternalInputBlocker(Monitor);}
        SetView(0);
    }
    Monitor->Refresh();
}
void ACoilLoadingMonitorHost::SetSceneRect(FSlateRect R,FVector2D F,bool Visible)
{if(Extension&&F.X>0&&F.Y>0){Extension->Rect=Visible?FVector4(R.Left/F.X,R.Top/F.Y,R.Right/F.X,R.Bottom/F.Y):FVector4(0,0,.001,.001);Extension->bPointOnly=Simulation&&Simulation->View()==4;}}
void ACoilLoadingMonitorHost::SetView(int32 View)
{
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||!Simulation||!ViewCamera)return;
    if(Coordinator)Coordinator->SetPointCloudOnlyMode(false);
    for(auto C:HiddenByView)if(C.IsValid())PC->HiddenPrimitiveComponents.Remove(C);HiddenByView.Reset();
    Simulation->SetView(View);FVector Position,Target;
    if(View==0){Position=FVector(-105,-120,46);Target=FVector(25,-4,16);}
    else
    {
        Target=Simulation->PreviewPlacement().Center;
        if(!Simulation->Plan().IsEmpty()){Target=FVector::ZeroVector;for(const auto& P:Simulation->Plan())Target+=P.Center;Target/=Simulation->Plan().Num();}
        Target.Z=FMath::Max(2.5,Target.Z);Position=Target+FVector(8,-18,11);
    }
    if(View==1)
    {
        TInlineComponentArray<UPrimitiveComponent*> Components(Simulation);
        for(auto* C:Components)
        {
            const FVector Local=Simulation->GetActorTransform().InverseTransformPosition(C->Bounds.Origin)*.01;
            if(Local.Y< -Simulation->Settings.HoldWidth*.5-.01||Local.X>Simulation->Settings.HoldLength*.5+.5||Local.X< -Simulation->Settings.HoldLength*.5-.5||Local.Z>Simulation->Settings.HoldHeight+Simulation->Settings.Coaming+.5)
            {if(!PC->HiddenPrimitiveComponents.Contains(C)){PC->HiddenPrimitiveComponents.Add(C);HiddenByView.Add(C);}}
        }
        for(TActorIterator<ACoilLoadingBody> It(GetWorld());It;++It)if(It->GetOwner()==Simulation)
        {
            const FVector P=Simulation->GetActorTransform().InverseTransformPosition(It->GetActorLocation())*.01;
            if(FMath::Abs(P.Y)>Simulation->Settings.HoldWidth*.5||FMath::Abs(P.X)>Simulation->Settings.HoldLength*.5||P.Z>Simulation->Settings.HoldHeight+1)
            {TInlineComponentArray<UPrimitiveComponent*> Parts(*It);for(auto* C:Parts)if(!PC->HiddenPrimitiveComponents.Contains(C)){PC->HiddenPrimitiveComponents.Add(C);HiddenByView.Add(C);}}
        }
    }
    ViewCamera->SetActorLocationAndRotation(Simulation->GetActorTransform().TransformPosition(Position*100),Simulation->GetActorQuat()*(Target-Position).Rotation().Quaternion());
    auto* Camera=ViewCamera->GetCameraComponent();Camera->FieldOfView=60;Camera->bConstrainAspectRatio=false;Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;Camera->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->PostProcessSettings.AutoExposureBias=.25;
    PC->SetViewTarget(ViewCamera);if(Coordinator)Coordinator->SetPointCloudOnlyMode(View==4);
}
void ACoilLoadingMonitorHost::Focus(int32 P)
{
    if(!Simulation||!ViewCamera)return;SetView(P==1?1:0);FVector Position,Target;
    if(P==0){Position={45,Simulation->QuayOffset(23),Simulation->QuayZ()+1.8};Target={-8,Simulation->Settings.RailCenterY(),Simulation->QuayZ()+17.5};ViewCamera->GetCameraComponent()->SetFieldOfView(72);}
    else if(P==1)
    {
        Position={-Simulation->Settings.HoldLength*.5-4,-Simulation->Settings.HoldWidth*.5-2.5,6};Target={-Simulation->Settings.HoldLength*.5+1.45,-Simulation->Settings.HoldWidth*.5+2.9,1.5};
        if(auto* PC=GetWorld()->GetFirstPlayerController())
        {TInlineComponentArray<UPrimitiveComponent*> Cs(Simulation);for(auto* C:Cs)if(Simulation->GetActorTransform().InverseTransformPosition(C->Bounds.Origin).X*.01< -Simulation->Settings.HoldLength*.5-.01&&!PC->HiddenPrimitiveComponents.Contains(C)){PC->HiddenPrimitiveComponents.Add(C);HiddenByView.Add(C);}}
    }
    else {Position={-23,Simulation->ShedCenterY()-Simulation->Settings.QuaySide*12,Simulation->QuayZ()+4};Target={0,Simulation->ShedCenterY(),Simulation->QuayZ()+2.3};}
    ViewCamera->SetActorLocationAndRotation(Simulation->GetActorTransform().TransformPosition(Position*100),Simulation->GetActorQuat()*(Target-Position).Rotation().Quaternion());
}
void ACoilLoadingMonitorHost::ShowTools()
{
    if(!Coordinator)return;if(!Tools){Tools=GetWorld()->SpawnActorDeferred<AVirtualSensorUiHostActor>(AVirtualSensorUiHostActor::StaticClass(),FTransform::Identity,this);Tools->SetFlags(RF_Transient);Tools->SensorManager=Coordinator;Tools->bAutoCreateOnBeginPlay=false;Tools->bAutoCreateToolPanels=false;Tools->FinishSpawning(FTransform::Identity);}
    Tools->CreateAndBindMonitorWidget();Tools->CreateAndBindToolWidgets();
}
void ACoilLoadingMonitorHost::EndPlay(const EEndPlayReason::Type R)
{
    GetWorldTimerManager().ClearTimer(Timer);if(Coordinator)Coordinator->SetPointCloudOnlyMode(false);Extension.Reset();
    if(auto* PC=GetWorld()->GetFirstPlayerController()){for(auto C:HiddenByView)PC->HiddenPrimitiveComponents.Remove(C);if(PreviousView.IsValid())PC->SetViewTarget(PreviousView.Get());PC->bShowMouseCursor=bPreviousCursor;}
    if(Monitor){if(auto* D=GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>()){D->UnregisterExternalInputBlocker(Monitor);D->UnregisterWidget(Monitor);}Monitor->RemoveFromParent();}
    if(Monitor&&GEngine)GEngine->bEnableOnScreenDebugMessagesDisplay=bPreviousDebugText;
    if(Tools)Tools->Destroy();if(Coordinator)Coordinator->Destroy();Super::EndPlay(R);
}
