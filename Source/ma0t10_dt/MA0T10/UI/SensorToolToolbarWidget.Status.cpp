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
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"

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
    const float FontScale=W->GetOwnedPanelFontScale();
    if(!FMath::IsNearlyEqual(DisplayedFontScale,FontScale))
    {
        DisplayedFontScale=FontScale;
        OwnedFontScaleText=FText::Format(NSLOCTEXT("SensorToolUiText","BottomFontScale","글자 {0}%"),FText::AsNumber(FMath::RoundToInt(FontScale*100)));
    }
    GlobalStatus=FSensorToolGlobalStatus();
    SlabCardTitle=FText::FromString(TEXT("Slab · 대기"));SlabCardAngle=SlabCardOffset=SlabCardMargin=FText::FromString(TEXT("—"));SlabCardProgress=0;
    OwnedSensorLabels.Reset();
    EquipmentLabels.Reset();
    auto* C=W->GetCoordinator();
    const auto* Scheduler=GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>();
    const auto* Rates=Scheduler?&Scheduler->GetTelemetry().SensorRates:nullptr;
    if(C)for(auto* A:C->GetSensorActors())if(IsValid(A)&&!A->IsActorBeingDestroyed())
    {
        ++GlobalStatus.EquipmentCount;
        const bool Running=A->IsSensorRunning();
        GlobalStatus.MeasuringCount+=Running?1:0;
        FText State=Running?SensorToolUiText::Measuring():SensorToolUiText::Stopped();
        FLinearColor Color=Running?FLinearColor(.15f,.85f,.4f):FSensorOwnedPresentationStyle::Muted();
        const bool Lidar=A->GetSensorKind()==EVirtualSensorKind::Lidar;
        if(Rates)if(const auto* Rate=Rates->FindByPredicate([&](const auto& R){return R.SensorId==A->GetSensorId()&&R.SensorKind==(Lidar?TEXT("LiDAR"):TEXT("Camera"));}))
        {
            if(Running&&Rate->State==EVirtualSensorRateState::Starved){State=SensorToolUiText::Starved();Color=FLinearColor(1,.6f,.1f);}
            else if(Rate->State==EVirtualSensorRateState::Failed){State=SensorToolUiText::Error();Color=FLinearColor(1,.15f,.2f);}
            else if(Rate->State==EVirtualSensorRateState::Paused)State=SensorToolUiText::Paused();
        }
        FEquipmentLabel Label;Label.Name=FText::FromString(FString(Lidar?TEXT("LiDAR "):TEXT("Camera "))+A->GetSensorId());Label.State=State;Label.Color=Color;
        EquipmentLabels.Add(A->GetSensorId(),Label);
        OwnedSensorLabels.Add(A->GetSensorId(),FText::Format(NSLOCTEXT("SensorToolUiText","CompactEquipmentLabel","{0} ● {1}"),Label.Name,State));
    }
    const auto* SessionService=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
    const auto Session=SessionService?SessionService->GetSlabSensorSessionStatus():FVirtualSlabSessionStatus();
    bool TransferFailure=false, PcdOverloadSuspended=false;
    if(C&&C->StreamPublisherComponent)for(const auto& Stream:C->StreamPublisherComponent->GetStreamStatuses())
    {
        if(Stream.StreamKind!=EVirtualSensorStreamKind::PointCloud)continue;
        const auto Config=C->StreamPublisherComponent->GetEffectiveStreamConfig(Stream.StreamKind,Stream.SensorId);
        if(Config.PointCloudFormat!=EVirtualPointCloudStreamFormat::PCD)continue;
        TransferFailure|=Stream.bOverloaded; // Historical failure counters are not a current blocker.
        PcdOverloadSuspended|=Stream.bOverloadSuspended;
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
    const bool HasServerSettings=C&&C->SharedTransportComponent&&!C->SharedTransportComponent->GetTransportProfile().BrokerUrl.TrimStartAndEnd().IsEmpty();
    if(GlobalStatus.bServerConnected||!HasServerSettings)ServerDisconnectedSince=-1;
    else if(ServerDisconnectedSince<0)ServerDisconnectedSince=Now;
    const auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    const bool SendDelayed=TransferFailure||(Raw&&Raw->IsSendDelayed());
    GlobalStatus.Equipment=SensorToolUiText::EquipmentCount(GlobalStatus.EquipmentCount,GlobalStatus.MeasuringCount);
    GlobalStatus.Pcd=PcdOverloadSuspended?SensorToolUiText::PcdOverloadSuspended():SensorToolUiText::PcdState(GlobalStatus.bPcdSending);
    GlobalStatus.Server=SensorToolUiText::ServerState(GlobalStatus.bServerConnected);
    GlobalStatus.Scenario=SensorToolUiText::ScenarioWaiting();
    if(auto* Progress=Cast<USlabProgressPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::SlabProgress)))StatusSlab=Progress->GetBoundSlabActor();
    if(!StatusSlab.IsValid()&&Now>=NextSlabLookup)
    {
        NextSlabLookup=Now+1;
        // Only an unambiguous actor is a valid fallback when no owned panel is bound.
        ASlabActor* Only=nullptr;int32 Count=0;
        for(TActorIterator<ASlabActor> It(GetWorld());It;++It){Only=*It;++Count;}
        CachedSlabCount=Count;
        if(Count==1)StatusSlab=Only;
    }
    if(!StatusSlab.IsValid())SlabCardTitle=FText::FromString(CachedSlabCount==0?TEXT("Slab이 없습니다 · 배치 방법"):TEXT("Slab이 여러 개입니다 · 대상 선택"));
    if(StatusSlab.IsValid())
    {
        const auto Simulation=StatusSlab->GetSimulationStatus();
        SlabCardProgress=Simulation.Progress;
        if(W->IsSidebarMode()&&Simulation.RowCount>0&&!Simulation.ScenarioUUID.IsEmpty())
        {
            SlabCardTitle=FText::FromString(FString::Printf(TEXT("Slab %s · %.0f%%"),*Simulation.MtlNo,Simulation.Progress*100));
            const auto Metrics=StatusSlab->GetCurrentMetrics();
            SlabCardAngle=FText::FromString(FString::Printf(TEXT("%.2f°"),Metrics.LeftAngle));
            SlabCardOffset=FText::FromString(FString::Printf(TEXT("%.1f cm"),Metrics.CenterOffsetCm));
            SlabCardMargin=Metrics.bMarginsValid?FText::FromString(FString::Printf(TEXT("%.1f / %.1f cm"),Metrics.MarginLeftCm,Metrics.MarginRightCm)):FText::FromString(TEXT("—"));
        }
        if(Simulation.State==ESlabSimulationState::Playing)GlobalStatus.Scenario=SensorToolUiText::ScenarioRunning(Simulation.Progress);
        else if(Simulation.State==ESlabSimulationState::Paused)GlobalStatus.Scenario=SensorToolUiText::ScenarioPaused();
        else if(Simulation.State==ESlabSimulationState::Preparing)GlobalStatus.Scenario=SensorToolUiText::ScenarioPreparing();
    }
    if(LabelExclusionSlab!=StatusSlab&&LabelExclusionSlab.IsValid())
        if(auto* V=LabelExclusionSlab->FindComponentByClass<USlabVisualizationComponent>())V->SetLabelExclusionRects(nullptr,{});
    LabelExclusionSlab=StatusSlab;
    if(StatusSlab.IsValid())if(auto* V=StatusSlab->FindComponentByClass<USlabVisualizationComponent>())V->SetLabelExclusionRects(this,W->GetOwnedDiagnosticExclusionRects());
    if(Session.State==EVirtualSlabSessionState::Draining)GlobalStatus.Scenario=SensorToolUiText::ScenarioDraining();
    else if(Session.State==EVirtualSlabSessionState::Preparing||Session.State==EVirtualSlabSessionState::Ready)GlobalStatus.Scenario=SensorToolUiText::ScenarioPreparing();
    // A single actionable summary; technical details stay in the destination panel.
    if(GlobalStatus.EquipmentCount==0)GlobalStatus.Warning=SensorToolUiText::EquipmentRequired();
    else if(!Preflight.BlockingReasons.IsEmpty())
    {
        GlobalStatus.Warning=FText::FromString(SensorToolUiText::PreflightReason(Preflight.BlockingReasons[0]));
    }
    else if(!GlobalStatus.bServerConnected)GlobalStatus.Warning=SensorToolUiText::ConnectionWarning(HasServerSettings,Now-ServerDisconnectedSince);
    else if(PcdOverloadSuspended)GlobalStatus.Warning=SensorToolUiText::PcdOverloadSuspended();
    else if(SendDelayed)GlobalStatus.Warning=SensorToolUiText::SendDelayed();
    else if(!Session.RequiredDataError.IsEmpty())GlobalStatus.Warning=SensorToolUiText::TransferCheck();
}

TSharedRef<SWidget> USensorToolToolbarWidget::BuildStatusStrip()
{
    auto Row=SNew(SHorizontalBox);
    static const FButtonStyle Chip=FButtonStyle(FSensorOwnedPresentationStyle::HeaderButton())
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,6.f))
        .SetHovered(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),6.f));
    static const FButtonStyle WarningChip=FButtonStyle(Chip).SetNormal(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::WarningSurface(),6.f));
    const ESensorToolDestination Destinations[]={ESensorToolDestination::Placement,ESensorToolDestination::PcdTransfer,ESensorToolDestination::Scenario,ESensorToolDestination::ServerSettings,ESensorToolDestination::Diagnostics};
    for(int32 I=0;I<UE_ARRAY_COUNT(Destinations);++I)
    {
        const auto Destination=Destinations[I];
        auto Label=[this,I]()
        {
            switch(I){case 0:return GlobalStatus.Equipment;case 1:return GlobalStatus.Pcd;case 2:return GlobalStatus.Scenario;case 3:return GlobalStatus.Server;default:return SensorToolUiText::Warning(GlobalStatus.Warning);}
        };
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(I==0?0:4,0,0,0)
        [SNew(SButton).ButtonStyle(I==4?&WarningChip:&Chip).ContentPadding(FMargin(8,3))
         .Visibility_Lambda([this,I](){return (I==4&&GlobalStatus.Warning.IsEmpty())||(I==3&&!GlobalStatus.bServerConnected)?EVisibility::Collapsed:EVisibility::Visible;})
         .ToolTipText_Lambda(Label).OnClicked_Lambda([this,Destination,I](){OpenOwnedDestination(I==4&&!GlobalStatus.bServerConnected?ESensorToolDestination::ServerSettings:Destination);return FReply::Handled();})
         [SNew(SBox).MaxDesiredWidth(I==4?340.f:240.f)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).Text_Lambda(Label).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
          .ColorAndOpacity(I==4?FSensorOwnedPresentationStyle::Warning():FSensorOwnedPresentationStyle::Muted())]]];
    }
    Row->AddSlot().FillWidth(1)[SNullWidget::NullWidget];
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).ButtonStyle(&Chip).HasDownArrow(false)
      .OnGetMenuContent_Lambda([this](){return BuildAppearanceMenu();}).ButtonContent()[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted())
      .Text_Lambda([this](){return OwnedFontScaleText;})]];
    return Row;
}
