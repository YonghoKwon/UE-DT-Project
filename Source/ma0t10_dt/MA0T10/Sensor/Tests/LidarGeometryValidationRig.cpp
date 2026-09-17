#include "LidarGeometryValidationRig.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"

ALidarGeometryValidationRig::ALidarGeometryValidationRig(){PrimaryActorTick.bCanEverTick=true;}
void ALidarGeometryValidationRig::BeginPlay(){Super::BeginPlay();}
void ALidarGeometryValidationRig::Tick(float Delta)
{
    Super::Tick(Delta);
    if(!Sensor || !MovingObject || !Plate) return;
    if(!bInitialized)
    {
        auto* PC=GetWorld()->GetFirstPlayerController();
        if(!PC || !GetWorld()->GetGameViewport()) return;
        auto* Scan=Sensor->ScanComponent.Get();
        Sensor->StopSensor();
        // Explicitly owned test fixture. Never applies these settings to a production actor.
        Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);
        Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
        Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
        Scan->bEnableRangeNoise=false;
        Scan->bEnableSemanticClassification=false;
        Scan->SetPointCloudPreviewEnabled(false);
        if(Coordinator) Coordinator->RegisterLidar(Scan);
        Monitor=CreateWidget<UVirtualSensorMonitorPanelWidget>(PC);
        Monitor->bPersistMonitorPreferences=false;
        Monitor->SetPanelPersistenceKey(NAME_None);
        Monitor->BindSensorManager(Coordinator);
        Monitor->BindVirtualLidar(Scan);
        Monitor->ShowLidarView();
        Monitor->ConfigurePanelLayout(EVirtualSensorPanelPlacement::RightCenter,FVector2D(900,680),18);
        Monitor->AddToViewport(10);
        FVirtualLidarVisualizationSettings S;
        S.ProjectionMode=ELidarMonitorProjectionMode::WorldTopDown;
        S.ColorMode=ELidarColorMode::GeometrySeparation;
        S.bShowGrid=false; S.bShowDepthEdges=false; S.PointSize=3;
        S.bShowWorldPointCloud=false;
        Sensor->VisualizationComponent->SetVisualizationSettings(S);
        Monitor->SetLidarColorMode(ELidarColorMode::GeometrySeparation);
        Sensor->StartSensor();
        if(ObservationCamera) PC->SetViewTarget(ObservationCamera);
        PC->bShowMouseCursor=true;
        PC->SetInputMode(FInputModeGameAndUI());
        auto Button=[this](const TCHAR* Text,TFunction<void()> Action)
        {
            return SNew(SButton).Text(FText::FromString(Text)).OnClicked_Lambda([Action](){Action();return FReply::Handled();});
        };
        Controls=SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
            +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).Padding(8).BorderBackgroundColor(FLinearColor(.02,.03,.05,1))[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("태그 없는 XYZ 검증 | 동일 재질 | 기준면 파랑 · 돌출 주황")))]
                +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("CPU / GPU"),[this](){ToggleBackend();})]
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("이동 / 정지"),[this](){bMoving=!bMoving;})]
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("형상 / 높이"),[this](){ToggleDisplay();})]
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("3D 포인트"),[this](){auto* V=Sensor->VisualizationComponent.Get();V->SetWorldPointCloudEnabled(!V->Settings.bShowWorldPointCloud);})]
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("검증 화면 저장"),[this](){SaveEvidence();})]
                ]
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(Status);})]
            ]]
            +SVerticalBox::Slot().FillHeight(1)[SNullWidget::NullWidget];
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(Controls.ToSharedRef(),20);
        bInitialized=true;
    }
    if(bMoving) Elapsed+=Delta;
    MovingObject->SetActorLocation(FVector(0,150*FMath::Sin(Elapsed*2*PI/30),12.5));
    UiElapsed+=Delta;
    if(UiElapsed>=.2f)
    {
        UiElapsed=0;
        const auto& D=Sensor->VisualizationComponent->GetGeometryDiagnostics();
        Status=FString::Printf(TEXT("%s · %s · 측정 %.1fHz · %s · 판 %d / 물체 %d / 불명 %d · %.2fms"),
            Sensor->ScanComponent->AcquisitionBackend==EVirtualLidarAcquisitionBackend::GpuDepthProjection?TEXT("GPU"):TEXT("CPU"),
            bMoving?TEXT("이동 중"):TEXT("정지"),Sensor->ScanComponent->GetRuntimeStatus().MeasuredCompletionRateHz,
            *D.Reason,D.PlanePoints,D.ProtrusionPoints,D.UnknownPoints,D.ProcessingMs);
    }
}
void ALidarGeometryValidationRig::ToggleBackend()
{
    Sensor->StopSensor();
    auto* S=Sensor->ScanComponent.Get();
    S->AcquisitionBackend=S->AcquisitionBackend==EVirtualLidarAcquisitionBackend::GpuDepthProjection?EVirtualLidarAcquisitionBackend::AccurateCpuTrace:EVirtualLidarAcquisitionBackend::GpuDepthProjection;
    Sensor->StartSensor();
}
void ALidarGeometryValidationRig::ToggleDisplay()
{
    auto* V=Sensor->VisualizationComponent.Get();
    Monitor->SetLidarColorMode(V->Settings.ColorMode==ELidarColorMode::GeometrySeparation?ELidarColorMode::RelativeHeight:ELidarColorMode::GeometrySeparation);
}
void ALidarGeometryValidationRig::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Monitor) Monitor->RemoveFromParent();
    if(Controls && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Controls.ToSharedRef());
    Controls.Reset();
    Super::EndPlay(Reason);
}

void ALidarGeometryValidationRig::SaveEvidence()
{
    auto* Viewport=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if(!Viewport||!Viewport->Viewport)return;
    const FString Root=FPaths::ProjectSavedDir()/TEXT("Reports");
    IFileManager::Get().MakeDirectory(*Root,true);
    const FIntPoint Size=Viewport->Viewport->GetSizeXY();
    const FString Report=FString::Printf(TEXT("# Actual mouse evidence\n\nViewport: %d x %d\nTags empty: %d\nSame material: %d\n%s\n"),Size.X,Size.Y,
        Plate->Tags.IsEmpty()&&MovingObject->Tags.IsEmpty(),Plate->GetStaticMeshComponent()->GetMaterial(0)==MovingObject->GetStaticMeshComponent()->GetMaterial(0),*Status);
    FFileHelper::SaveStringToFile(Report,*(Root/TEXT("geometry_manual.md")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FScreenshotRequest::RequestScreenshot(Root/TEXT("geometry_manual.png"),true,false);
}
