#include "SensorToolToolbarWidget.h"
#include "SensorToolUiText.h"
#include "SensorOwnedPresentationStyle.h"
#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

void USensorToolToolbarWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    RefreshOwnedStatus(FPlatformTime::Seconds());
}

void USensorToolToolbarWidget::RefreshOwnedStatus(double Now)
{
    auto* W=Workspace();
    if(!W||!W->OwnsToolbar(this)||Now<NextStatusRefresh)return;
    NextStatusRefresh=Now+.2;
    GlobalStatus=FSensorToolGlobalStatus();
    OwnedSensorLabels.Reset();
    auto* C=W->GetCoordinator();
    const auto* Scheduler=GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>();
    const auto* Rates=Scheduler?&Scheduler->GetTelemetry().SensorRates:nullptr;
    if(C)for(auto* A:C->GetSensorActors())if(IsValid(A)&&!A->IsActorBeingDestroyed())
    {
        ++GlobalStatus.EquipmentCount;
        const bool Running=A->IsSensorRunning();
        GlobalStatus.MeasuringCount+=Running?1:0;
        FText State=Running?SensorToolUiText::Measuring():SensorToolUiText::Stopped();
        FString Dot=Running?TEXT("●"):TEXT("○");
        const bool Lidar=A->GetSensorKind()==EVirtualSensorKind::Lidar;
        if(Rates)if(const auto* Rate=Rates->FindByPredicate([&](const auto& R){return R.SensorId==A->GetSensorId()&&R.SensorKind==(Lidar?TEXT("LiDAR"):TEXT("Camera"));}))
        {
            if(Rate->State==EVirtualSensorRateState::Starved){State=SensorToolUiText::Starved();Dot=TEXT("●");}
            else if(Rate->State==EVirtualSensorRateState::Failed){State=SensorToolUiText::Error();Dot=TEXT("●");}
            else if(Rate->State==EVirtualSensorRateState::Paused)State=SensorToolUiText::Paused();
        }
        OwnedSensorLabels.Add(A->GetSensorId(),FText::Format(NSLOCTEXT("SensorToolUiText","EquipmentLabel","{0} · {1} {2} {3}"),
            FText::FromString(Lidar?TEXT("LiDAR"):TEXT("Camera")),FText::FromString(A->GetSensorId()),FText::FromString(Dot),State));
    }
    const auto* SessionService=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
    const auto Session=SessionService?SessionService->GetSlabSensorSessionStatus():FVirtualSlabSessionStatus();
    bool TransferFailure=false;
    if(C&&C->StreamPublisherComponent)for(const auto& Stream:C->StreamPublisherComponent->GetStreamStatuses())
    {
        if(Stream.StreamKind!=EVirtualSensorStreamKind::PointCloud)continue;
        const auto Config=C->StreamPublisherComponent->GetEffectiveStreamConfig(Stream.StreamKind,Stream.SensorId);
        if(Config.PointCloudFormat!=EVirtualPointCloudStreamFormat::PCD)continue;
        TransferFailure|=Stream.bOverloaded; // Historical failure counters are not a current blocker.
    }
    if(C&&C->StreamPublisherComponent)for(auto* A:C->GetSensorActors())if(IsValid(A)&&A->GetSensorKind()==EVirtualSensorKind::Lidar)
    {
        FVirtualSensorStreamConfig Config;
        if(!C->StreamPublisherComponent->ResolveActiveStreamForSensor(EVirtualSensorStreamKind::PointCloud,A->GetSensorId(),Config)||Config.PointCloudFormat!=EVirtualPointCloudStreamFormat::PCD)continue;
        const bool Controlled=SessionService&&SessionService->ControlsSensor(A->GetSensorId());
        GlobalStatus.bPcdSending|=!Controlled||(Session.Outputs.bPointCloud&&Session.State==EVirtualSlabSessionState::Running&&Session.CurrentSlab.bEligible);
    }
    GlobalStatus.bPcdSending|=Session.State==EVirtualSlabSessionState::Draining&&Session.Outputs.bPointCloud&&Session.UnfinishedFrames>0;
    const auto Preflight=UVirtualSensorOutputPreflight::InspectForPurpose(this,FVirtualSlabSensorOutputSelection(),{},EVirtualSensorPreflightPurpose::ScenarioTransition);
    GlobalStatus.bServerConnected=Preflight.bTransportConnected;
    GlobalStatus.Equipment=SensorToolUiText::EquipmentCount(GlobalStatus.EquipmentCount,GlobalStatus.MeasuringCount);
    GlobalStatus.Pcd=SensorToolUiText::PcdState(GlobalStatus.bPcdSending);
    GlobalStatus.Server=SensorToolUiText::ServerState(GlobalStatus.bServerConnected);
    GlobalStatus.Scenario=SensorToolUiText::ScenarioWaiting();
    if(auto* Progress=Cast<USlabProgressPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::SlabProgress)))StatusSlab=Progress->GetBoundSlabActor();
    if(!StatusSlab.IsValid()&&Now>=NextSlabLookup)
    {
        NextSlabLookup=Now+1;
        // Only an unambiguous actor is a valid fallback when no owned panel is bound.
        ASlabActor* Only=nullptr;int32 Count=0;
        for(TActorIterator<ASlabActor> It(GetWorld());It;++It){Only=*It;++Count;}
        if(Count==1)StatusSlab=Only;
    }
    if(StatusSlab.IsValid())
    {
        const auto Simulation=StatusSlab->GetSimulationStatus();
        if(Simulation.State==ESlabSimulationState::Playing)GlobalStatus.Scenario=SensorToolUiText::ScenarioRunning(Simulation.Progress);
        else if(Simulation.State==ESlabSimulationState::Paused)GlobalStatus.Scenario=SensorToolUiText::ScenarioPaused();
        else if(Simulation.State==ESlabSimulationState::Preparing)GlobalStatus.Scenario=SensorToolUiText::ScenarioPreparing();
    }
    if(Session.State==EVirtualSlabSessionState::Draining)GlobalStatus.Scenario=SensorToolUiText::ScenarioDraining();
    else if(Session.State==EVirtualSlabSessionState::Preparing||Session.State==EVirtualSlabSessionState::Ready)GlobalStatus.Scenario=SensorToolUiText::ScenarioPreparing();
    // A single actionable summary; technical details stay in the destination panel.
    if(GlobalStatus.EquipmentCount==0)GlobalStatus.Warning=SensorToolUiText::EquipmentRequired();
    else if(!Preflight.BlockingReasons.IsEmpty())
    {
        FString Reason=Preflight.BlockingReasons[0];
        Reason.ReplaceInline(TEXT("센서 Coordinator"),TEXT("장비 관리"));
        Reason.ReplaceInline(TEXT("SensorId"),TEXT("장비 ID"));
        Reason.ReplaceInline(TEXT("Broker URL"),TEXT("서버 주소"));
        Reason.ReplaceInline(TEXT("실시간 Topic 송신에는 STOMP 전송 모드"),TEXT("실시간 전송에는 서버 연결 방식"));
        Reason.ReplaceInline(TEXT("Topic"),TEXT("전송 경로"));
        GlobalStatus.Warning=FText::FromString(Reason);
    }
    else if(!GlobalStatus.bServerConnected)GlobalStatus.Warning=SensorToolUiText::ConnectionRequired();
    else if(TransferFailure||!Session.RequiredDataError.IsEmpty())GlobalStatus.Warning=SensorToolUiText::TransferCheck();
}

