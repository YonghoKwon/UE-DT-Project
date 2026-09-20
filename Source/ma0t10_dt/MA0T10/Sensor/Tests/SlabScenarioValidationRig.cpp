#include "SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "Core/DxDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "UnrealClient.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "Editor/EditorPerformanceSettings.h"
#endif

ASlabScenarioValidationRig::ASlabScenarioValidationRig(){PrimaryActorTick.bCanEverTick=true;}
void ASlabScenarioValidationRig::ConfigureLocalBroker()
{
    if(!Coordinator||!Coordinator->SharedTransportComponent)return;
    auto Profile=Coordinator->SharedTransportComponent->GetTransportProfile();
    Profile.BrokerUrl=TEXT("ws://127.0.0.1:61616");
    Profile.UserName=TEXT("artemis");
    Coordinator->SharedTransportComponent->ConfigureTransportProfile(Profile);
    Coordinator->SharedTransportComponent->SetSessionCredentials(TEXT("artemis"),FString());
    Coordinator->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
}
void ASlabScenarioValidationRig::Tick(float Delta)
{
    Super::Tick(Delta);
    if(!SlabActor||!Lidar||!Coordinator)return;
    if(!bInitialized)
    {
        auto* PC=GetWorld()->GetFirstPlayerController();
        if(!PC||!GetWorld()->GetGameViewport())return;
        // Owned validation fixture only: production UI uses the Actor editable-state API.
        Lidar->StopSensor();
        auto* Scan=Lidar->ScanComponent.Get();
        Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);
        Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Scan->SensorId=TEXT("SLAB-LIDAR-001");
        Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
        Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
        Scan->bEnableRangeNoise=false;
        Scan->SetServerPayloadPolicy(32,1024,false);
        Coordinator->RegisterLidar(Scan);
        Lidar->SetSharedOutputServices(Coordinator->SharedTransportComponent,Coordinator->SharedRecorderComponent,Coordinator->StreamPublisherComponent);
        Lidar->StartSensor();
        if(Camera)
        {
            Camera->StopSensor();
            Camera->CaptureComponent->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);
            Camera->CaptureComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
            Coordinator->RegisterCamera(Camera->CaptureComponent);
            Camera->SetSharedOutputServices(Coordinator->SharedTransportComponent,Coordinator->SharedRecorderComponent,Coordinator->StreamPublisherComponent);
        }
        if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_LOCAL_BROKER"))==TEXT("1"))ConfigureLocalBroker();
        PC->bShowMouseCursor=true;PC->SetInputMode(FInputModeGameAndUI());
        // Explicit test-only comparison switch; never changes production defaults or saves actors.
        if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_ANALYSIS_OFF"))==TEXT("1"))
        {
            FSlabAnalysisDisplaySettings Off;
            Off.bOutline=Off.bCross=Off.bReferencePose=Off.bCenterline=Off.bYaw=Off.bMargins=Off.bStatus=false;
            SlabActor->SetAnalysisDisplaySettings(Off);
        }
        auto Button=[](const TCHAR* Label,TFunction<void()> Action){return SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda([Action](){Action();return FReply::Handled();});};
        Controls=SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
            +SVerticalBox::Slot().FillHeight(1)[SNullWidget::NullWidget]
            +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).Padding(6).BorderBackgroundColor(FLinearColor(.02,.03,.05,1))[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("검증: 30초 관찰"),[this](){SubmitSynthetic(false);})]
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("검증: 30초 PCD / 로컬 Broker"),[this](){SubmitSynthetic(true);})]
                    +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("검증 화면 저장"),[this](){SaveEvidence();})]
                ]
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(DisplayStatus);})]
            ]];
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(Controls.ToSharedRef(),70);
        bInitialized=true;
    }
    const auto S=SlabActor->GetSimulationStatus();
    if(S.RunUUID!=ActiveRun&&!S.RunUUID.IsEmpty()){ActiveRun=S.RunUUID;FrameTimes.Reset();bReportWritten=false;}
    if(S.State==ESlabSimulationState::Playing&&Delta>0)FrameTimes.Add(Delta*1000.0);
    if(!bReportWritten&&!ActiveRun.IsEmpty()&&(S.State==ESlabSimulationState::Completed||S.State==ESlabSimulationState::Failed))
    {
        const auto Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus();
        if(Session.State!=EVirtualSlabSessionState::Draining){WriteReport();bReportWritten=true;}
    }
    UiTime+=Delta;
    if(UiTime>=.2f){UiTime=0;DisplayStatus=FString::Printf(TEXT("전용 검증맵 · 치수 mm / 위치 cm · 실제 시점 조작은 PIE F8 또는 기본 Pawn | %s | Slab %lld · %.2f / %.2f초 · LiDAR %.1fHz"),*S.Message,S.FrameNo,S.ElapsedSec,S.DurationSec,Lidar->GetSensorRuntimeStatus().MeasuredCompletionRateHz);}
}
void ASlabScenarioValidationRig::SubmitSynthetic(bool bPcd)
{
    if(!SlabActor)return;
    if(bPcd)ConfigureLocalBroker();
    auto Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly();Outputs.bPointCloud=bPcd;
    if(!SlabActor->SetSensorOutputs(Outputs))return;
    const FString Json=FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/Slab");
    IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("last_dtcore_input.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    // Exercise DTCore queue -> worker ParseToStruct -> GT ProcessStructData, not direct actor injection.
    GetWorld()->GetGameInstance()->GetSubsystem<UDxDataSubsystem>()->EnqueueWebSocketData(Json);
}
void ASlabScenarioValidationRig::WriteReport()
{
    auto Root=MakeShared<FJsonObject>();const auto S=SlabActor->GetSimulationStatus();
    Root->SetStringField(TEXT("run_uuid"),S.RunUUID);Root->SetStringField(TEXT("scenario_uuid"),S.ScenarioUUID);
    Root->SetStringField(TEXT("message"),S.Message);Root->SetNumberField(TEXT("last_slab_frame"),S.FrameNo);
    Root->SetNumberField(TEXT("elapsed"),S.ElapsedSec);Root->SetNumberField(TEXT("duration"),S.DurationSec);
    Root->SetBoolField(TEXT("completed"),S.State==ESlabSimulationState::Completed);
    const auto Display=SlabActor->GetAnalysisDisplaySettings();
    Root->SetBoolField(TEXT("analysis_display_enabled"),Display.bOutline||Display.bCross||Display.bReferencePose||Display.bCenterline||Display.bYaw||Display.bMargins||Display.bStatus);
    if(auto* V=GetWorld()->GetGameViewport();V&&V->Viewport)
    {
        Root->SetNumberField(TEXT("viewport_width"),V->Viewport->GetSizeXY().X);
        Root->SetNumberField(TEXT("viewport_height"),V->Viewport->GetSizeXY().Y);
    }
#if WITH_EDITOR
    Root->SetBoolField(TEXT("editor_background_throttle"),GetDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground);
    Root->SetBoolField(TEXT("editor_performance_monitor"),GetDefault<UEditorPerformanceSettings>()->bMonitorEditorPerformance);
#endif
    if(!FrameTimes.IsEmpty())
    {
        auto Sorted=FrameTimes;Sorted.Sort();double Sum=0,Slow=0;for(double V:Sorted)Sum+=V;
        const int32 SlowCount=FMath::Max(1,FMath::CeilToInt(Sorted.Num()*.01));
        for(int32 I=Sorted.Num()-SlowCount;I<Sorted.Num();++I)Slow+=Sorted[I];
        Root->SetNumberField(TEXT("average_engine_fps"),1000*Sorted.Num()/Sum);
        Root->SetNumberField(TEXT("one_percent_low"),1000*SlowCount/Slow);
        Root->SetNumberField(TEXT("engine_frame_p95_ms"),Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.95)-1,0,Sorted.Num()-1)]);
        TArray<TSharedPtr<FJsonValue>> Samples;Samples.Reserve(FrameTimes.Num());for(double V:FrameTimes)Samples.Add(MakeShared<FJsonValueNumber>(V));Root->SetArrayField(TEXT("engine_frame_ms"),Samples);
    }
    TArray<TSharedPtr<FJsonValue>> Streams;
    for(const auto& T:GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->GetStreamTelemetry())
    {
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("sensor"),T.SensorId);R->SetNumberField(TEXT("kind"),int32(T.StreamKind));
        R->SetNumberField(TEXT("submit"),T.SubmittedCount);R->SetNumberField(TEXT("receipt"),T.ReceiptCount);R->SetNumberField(TEXT("consumer"),T.ConsumerReceivedCount);
        R->SetNumberField(TEXT("invalid"),T.ValidationFailureCount);R->SetNumberField(TEXT("gap"),T.FrameGapCount);R->SetNumberField(TEXT("overflow"),T.OverloadCount);
        Streams.Add(MakeShared<FJsonValueObject>(R));
    }
    Root->SetArrayField(TEXT("streams"),Streams);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/Slab");IFileManager::Get().MakeDirectory(*Dir,true);
    FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(Dir/(S.RunUUID+TEXT(".json"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void ASlabScenarioValidationRig::SaveEvidence(bool bScreenshot)
{
    WriteReport();const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/Slab");
    auto* V=GetWorld()->GetGameViewport();const auto Size=V&&V->Viewport?V->Viewport->GetSizeXY():FIntPoint::ZeroValue;
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("# Actual UI evidence\n\nViewport: %d x %d\n%s\n"),Size.X,Size.Y,*DisplayStatus),*(Dir/TEXT("manual.md")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if(bScreenshot)FScreenshotRequest::RequestScreenshot(Dir/TEXT("manual.png"),true,false);
}
void ASlabScenarioValidationRig::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Controls&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Controls.ToSharedRef());
    Controls.Reset();Super::EndPlay(Reason);
}
