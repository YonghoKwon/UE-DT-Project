#include "CoilLoadingMonitor.h"
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
        const auto Full=Data.GetViewRect();const int32 W=Full.Width(),H=Full.Height();
        FIntRect R(FMath::RoundToInt(W*Rect.X),FMath::RoundToInt(H*Rect.Y),FMath::RoundToInt(W*Rect.Z),FMath::RoundToInt(H*Rect.W));
        if(R.Width()<1||R.Height()<1)return;Data.SetViewRectangle(R);Data.ProjectionMatrix.M[1][1]=Data.ProjectionMatrix.M[0][0]*R.Width()/R.Height();
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
    if(View==0){Position=FVector(-115,-145,100);Target=FVector(20,-3,18);}
    else{Position=FVector(-37,-14,20);Target=FVector(-20,0,2.5);}
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
        Position=FVector(-14,-28,25);Target=FVector(-27,0,5);
    }
    ViewCamera->SetActorLocationAndRotation(Simulation->GetActorTransform().TransformPosition(Position*100),Simulation->GetActorQuat()*(Target-Position).Rotation().Quaternion());
    auto* Camera=ViewCamera->GetCameraComponent();Camera->FieldOfView=60;Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;Camera->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->PostProcessSettings.AutoExposureBias=.25;
    PC->SetViewTarget(ViewCamera);if(Coordinator)Coordinator->SetPointCloudOnlyMode(View==4);
}
void ACoilLoadingMonitorHost::Focus(int32 P)
{
    if(!Simulation||!ViewCamera)return;SetView(P==1?1:0);FVector Position,Target;
    if(P==0){Position={-38,-48,Simulation->QuayZ()+21};Target={-8,Simulation->QuayEdgeY()-2,Simulation->QuayZ()+14};}
    else if(P==1)
    {
        Position={-34,-11,6};Target={-28.845,-5.6,1.5};
        if(auto* PC=GetWorld()->GetFirstPlayerController())
        {TInlineComponentArray<UPrimitiveComponent*> Cs(Simulation);for(auto* C:Cs)if(Simulation->GetActorTransform().InverseTransformPosition(C->Bounds.Origin).X*.01< -Simulation->Settings.HoldLength*.5-.01&&!PC->HiddenPrimitiveComponents.Contains(C)){PC->HiddenPrimitiveComponents.Add(C);HiddenByView.Add(C);}}
    }
    else {Position={-23,Simulation->QuayEdgeY()-17,Simulation->QuayZ()+4};Target={0,Simulation->QuayEdgeY()-29,Simulation->QuayZ()+2.3};}
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
