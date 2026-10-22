#include "ShipHatchMonitorHost.h"
#include "ShipHatchMonitorWidget.h"
#include "VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "VirtualSensorMonitorPanelWidget.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "SceneViewExtension.h"
#include "SceneView.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

/** Restrict the existing player view, with no SceneCapture or offscreen texture. */
class FShipHatchViewExtension final : public FWorldSceneViewExtension
{
public:
    FShipHatchViewExtension(const FAutoRegister& Register,UWorld* World):FWorldSceneViewExtension(Register,World){}
    FVector4 Rect=FVector4(0,0,1,1);
    bool bPointOnly=false;
    virtual void SetupViewFamily(FSceneViewFamily&) override {}
    virtual void SetupView(FSceneViewFamily& Family,FSceneView& View) override
    {if(bPointOnly&&!View.bIsSceneCapture&&!View.bIsReflectionCapture)Family.EngineShowFlags.SetAtmosphere(false);}
    virtual void BeginRenderViewFamily(FSceneViewFamily&) override {}
    virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& Data) override
    {
        const auto Full=Data.GetViewRect();const int32 W=Full.Width(),H=Full.Height();
        FIntRect R(FMath::RoundToInt(W*Rect.X),FMath::RoundToInt(H*Rect.Y),FMath::RoundToInt(W*Rect.Z),FMath::RoundToInt(H*Rect.W));
        if(R.Width()<1||R.Height()<1)return;Data.SetViewRectangle(R);
        Data.ProjectionMatrix.M[1][1]=Data.ProjectionMatrix.M[0][0]*R.Width()/R.Height();
    }
};

AShipHatchMonitorHost::AShipHatchMonitorHost(){PrimaryActorTick.bCanEverTick=false;}
void AShipHatchMonitorHost::BeginPlay()
{
    Super::BeginPlay();GetWorldTimerManager().SetTimer(RefreshTimer,this,&AShipHatchMonitorHost::Refresh,.2f,true);
}
void AShipHatchMonitorHost::Refresh()
{
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||!IsValid(Simulation))return;
    if(!Monitor)
    {
        PreviousView=PC->GetViewTarget();bPreviousCursor=PC->bShowMouseCursor;
        ViewExtension=FSceneViewExtensions::NewExtension<FShipHatchViewExtension>(GetWorld());
        if(ViewCamera)
        {
            auto* Camera=ViewCamera->GetCameraComponent();PreviousPostProcess=Camera->PostProcessSettings;
            PreviousPostProcessWeight=Camera->PostProcessBlendWeight;bCameraExposureOverridden=true;
            // One map-owned player view: dark cargo must not trigger a brighter
            // auto exposure on close-up. Sensor capture/optical response stays independent.
            auto& Post=Camera->PostProcessSettings;Post.bOverride_AutoExposureMethod=true;Post.AutoExposureMethod=AEM_Manual;
            Post.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Post.AutoExposureApplyPhysicalCameraExposure=false;
            Post.bOverride_AutoExposureBias=true;Post.AutoExposureBias=.25f;Camera->PostProcessBlendWeight=1;
            PC->SetViewTarget(ViewCamera);
        }
        PC->bShowMouseCursor=true;
        FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);PC->SetInputMode(Mode);
        FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;
        Coordinator=GetWorld()->SpawnActor<AVirtualSensorCoordinator>(P);
        // The generic point-only view keeps one selected sensor. This map explicitly composes two previews.
        Coordinator->PointCloudOnlyKeepActorTags.AddUnique(TEXT("ShipHatchPreviewSensor"));
        Monitor=CreateWidget<UShipHatchMonitorWidget>(PC,UShipHatchMonitorWidget::StaticClass());
        Monitor->Bind(this,Simulation);Monitor->AddToViewport(20);
        if(auto* D=GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>())
        {D->RegisterWidget(Monitor);D->RegisterExternalInputBlocker(Monitor);}
    }
    if(!bRegisteredSensors&&Simulation->MLX&&Simulation->Livox&&(!Simulation->bFieldLayout||Simulation->CabRight))
    {
        Simulation->MLX->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));Simulation->Livox->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));
        Coordinator->RegisterSensorActor(Simulation->MLX);Coordinator->RegisterSensorActor(Simulation->Livox);if(Simulation->CabRight){Simulation->CabRight->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));Coordinator->RegisterSensorActor(Simulation->CabRight);}bRegisteredSensors=true;
    }
    Monitor->SetHatchPresentationVisible(Monitor->IsVisible());Monitor->Refresh();
    if(Simulation->CabRight&&!Simulation->CabRight->ActorHasTag(TEXT("ShipHatchPreviewSensor")))
    {Simulation->CabRight->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));Coordinator->RegisterSensorActor(Simulation->CabRight);}
    const bool ProjectionVisible=Tools&&Tools->GetMonitorWidget()&&Tools->GetMonitorWidget()->IsVisible();
    for(auto* A:{Simulation->MLX.Get(),Simulation->Livox.Get(),Simulation->CabRight.Get()})if(A)A->VisualizationComponent->SetProjectionPreviewEnabled(ProjectionVisible);
}
void AShipHatchMonitorHost::SetSceneRect(const FSlateRect& Rect,FVector2D Full,bool Visible)
{
    if(Full.X<=0||Full.Y<=0)return;
    if(ViewExtension){ViewExtension->Rect=Visible?FVector4(Rect.Left/Full.X,Rect.Top/Full.Y,Rect.Right/Full.X,Rect.Bottom/Full.Y):FVector4(0,0,.001,.001);ViewExtension->bPointOnly=Monitor&&Monitor->GetView()==1;}
}
void AShipHatchMonitorHost::ShowTools()
{
    if(!Coordinator)return;
    if(!Tools)
    {
        Tools=GetWorld()->SpawnActorDeferred<AVirtualSensorUiHostActor>(AVirtualSensorUiHostActor::StaticClass(),FTransform::Identity,this);
        if(!Tools)return;Tools->SetFlags(RF_Transient);Tools->SensorManager=Coordinator;
        Tools->bAutoCreateOnBeginPlay=false;Tools->bAutoCreateToolPanels=false;Tools->FinishSpawning(FTransform::Identity);
    }
    Tools->CreateAndBindMonitorWidget();Tools->CreateAndBindToolWidgets();
}
void AShipHatchMonitorHost::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimer);
    if(bCameraExposureOverridden&&IsValid(ViewCamera))
    {auto* Camera=ViewCamera->GetCameraComponent();Camera->PostProcessSettings=PreviousPostProcess;Camera->PostProcessBlendWeight=PreviousPostProcessWeight;bCameraExposureOverridden=false;}
    ViewExtension.Reset();
    if(Coordinator)Coordinator->SetPointCloudOnlyMode(false);
    if(Monitor)
    {
        if(auto* D=GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>())
        {D->UnregisterExternalInputBlocker(Monitor);D->UnregisterWidget(Monitor);}
        Monitor->RemoveFromParent();Monitor=nullptr;
    }
    if(Tools)Tools->Destroy();if(Coordinator)Coordinator->Destroy();
    if(auto* PC=GetWorld()->GetFirstPlayerController())
    {if(PreviousView.IsValid())PC->SetViewTarget(PreviousView.Get());PC->bShowMouseCursor=bPreviousCursor;}
    Super::EndPlay(Reason);
}
