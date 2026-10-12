#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolAppearance.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"

const FString UVirtualSensorToolWorkspaceSubsystem::SlotName=VirtualSensorDefaults::WorkspaceSlot;
float UVirtualSensorToolWorkspaceSubsystem::CalculateToolbarReservedTop(float DesiredHeight,float ViewportHeight)
{
	const float Height=FMath::IsFinite(DesiredHeight)?DesiredHeight:48;
	return 16+FMath::Clamp(Height,40.0f,FMath::Max(40.0f,ViewportHeight*.4f));
}
void UVirtualSensorToolWorkspaceSubsystem::SetCoordinator(AVirtualSensorCoordinator* C)
{
	if(Coordinator.Get()==C)return;
	if(auto* Old=Coordinator.Get()){Old->OnSensorInventoryChanged.RemoveAll(this);for(auto* A:Old->GetSensorActors())if(IsValid(A))A->SetPlacementPickingEnabled(false);}
	Coordinator=C;
	if(C)C->OnSensorInventoryChanged.AddUObject(this,&ThisClass::RefreshOwnedPickMarkers);
	RefreshOwnedPickMarkers();
	if(PlacementInteraction)PlacementInteraction->BindCoordinator(C);
}
void UVirtualSensorToolWorkspaceSubsystem::RefreshOwnedPickMarkers()
{
	TArray<TWeakObjectPtr<AVirtualSensorActorBase>> Current;
	if(auto* C=Coordinator.Get())for(auto* A:C->GetSensorActors())if(IsValid(A)){Current.Add(A);A->SetPlacementPickingEnabled(true);}
	for(const auto& Old:OwnedPickTargets)if(Old.IsValid()&&!Current.Contains(Old))Old->SetPlacementPickingEnabled(false);
	OwnedPickTargets=MoveTemp(Current);
}
bool UVirtualSensorToolWorkspaceSubsystem::RegisterPlacementHost(AVirtualSensorUiHostActor* Host)
{
	if(!IsValid(Host)||Host->GetWorld()!=GetWorld()||(PlacementHost.IsValid()&&PlacementHost.Get()!=Host))return false;
	PlacementHost=Host;return true;
}
void UVirtualSensorToolWorkspaceSubsystem::UnregisterPlacementHost(AVirtualSensorUiHostActor* Host)
{if(PlacementHost.Get()==Host)PlacementHost.Reset();}
AVirtualSensorCoordinator* UVirtualSensorToolWorkspaceSubsystem::GetCoordinator() const{return Coordinator.Get();}
void UVirtualSensorToolWorkspaceSubsystem::Initialize(FSubsystemCollectionBase& C)
{
	Super::Initialize(C);
	Preferences=Cast<USensorToolWorkspaceSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName,0));
	if(!Preferences||Preferences->Version!=1) {Preferences=NewObject<USensorToolWorkspaceSaveGame>(this);Preferences->FontScale=USensorToolAppearance::LoadScale();}
	if(!FMath::IsFinite(Preferences->FontScale))Preferences->FontScale=1;
	Preferences->FontScale=FMath::Clamp(Preferences->FontScale,.85f,1.5f);
}
void UVirtualSensorToolWorkspaceSubsystem::Deinitialize()
{
	if(PlacementInteraction){PlacementInteraction->Shutdown();PlacementInteraction=nullptr;}
	SetCoordinator(nullptr);
	for(auto& P:Panels)if(P.Value)P.Value->SetToolWorkspace(nullptr,P.Key);
	if(Root)Root->RemoveFromParent();Panels.Reset();PendingInitialLayouts.Reset();Root=nullptr;Canvas=nullptr;Toolbar=nullptr;Super::Deinitialize();
}
float UVirtualSensorToolWorkspaceSubsystem::GetOwnedPanelFontScale() const { return Preferences?Preferences->FontScale:1; }
UVirtualSensorPanelWidgetBase* UVirtualSensorToolWorkspaceSubsystem::GetOwnedPanel(ESensorToolPanelRole R) const {const auto* P=Panels.Find(R);return P?P->Get():nullptr;}
bool UVirtualSensorToolWorkspaceSubsystem::IsPanelOpen(ESensorToolPanelRole R) const {const auto* S=Preferences?Preferences->Panels.Find(R):nullptr;return S?S->bOpen:(R==ESensorToolPanelRole::Monitor||R==ESensorToolPanelRole::Placement);}
bool UVirtualSensorToolWorkspaceSubsystem::RegisterOwnedPanel(ESensorToolPanelRole R,UVirtualSensorPanelWidgetBase* P)
{
	if(!P||P->GetWorld()!=GetWorld()||!Preferences)return false;
	const bool Allowed=(R==ESensorToolPanelRole::Monitor&&P->IsA<UVirtualSensorMonitorPanelWidget>())||(R==ESensorToolPanelRole::Settings&&P->IsA<UVirtualSensorSettingsPanelWidget>())||(R==ESensorToolPanelRole::Data&&P->IsA<UVirtualSensorCaptureExportPanelWidget>())||(R==ESensorToolPanelRole::Replay&&P->IsA<USlabScenarioReplayPanelWidget>())||(R==ESensorToolPanelRole::SlabCharts&&P->IsA<USlabChartsPanelWidget>())||(R==ESensorToolPanelRole::SlabProgress&&P->IsA<USlabProgressPanelWidget>())||(R==ESensorToolPanelRole::Placement&&P->IsA<UVirtualSensorPlacementPanelWidget>());
	if(!Allowed)return false;
	if(auto* Existing=GetOwnedPanel(R))return Existing==P;
	Panels.Add(R,P);PendingInitialLayouts.Add(R);P->SetToolWorkspace(this,R);P->ApplySensorToolFontScale(GetOwnedPanelFontScale());
	if(auto* GI=GetWorld()->GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>())Dx->RegisterExternalInputBlocker(P);
	return true;
}
void UVirtualSensorToolWorkspaceSubsystem::EnsureRoot()
{
	if(Root||!GetWorld()->GetFirstPlayerController())return;
	Root=CreateWidget<UUserWidget>(GetWorld(),USensorToolWorkspaceRootWidget::StaticClass());
	Canvas=Root->WidgetTree->ConstructWidget<UCanvasPanel>();Root->WidgetTree->RootWidget=Canvas;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Toolbar=CreateWidget<UUserWidget>(GetWorld(),USensorToolToolbarWidget::StaticClass());
	if(auto* GI=GetWorld()->GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>())Dx->RegisterExternalInputBlocker(Toolbar);
	auto* S=Canvas->AddChildToCanvas(Toolbar);S->SetPosition(FVector2D(16,8));S->SetSize(FVector2D(1000,48));S->SetZOrder(10000);
	RefreshHosting();
}
void UVirtualSensorToolWorkspaceSubsystem::RefreshHosting()
{
	if(!Root)return;
	auto* Dx=GetWorld()->GetGameInstance()?GetWorld()->GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>():nullptr;
	auto* Main=Dx?Cast<UCanvasPanel>(Dx->GetAddWidgetPanel()):nullptr;
	if(Root->IsInViewport()&&!Main)return;
	if(Main&&Root->GetParent()==Main)return;
	// Retain Slate objects during rehosting, so active widget callbacks survive.
	const auto Pin=Root->TakeWidget();Root->RemoveFromParent();
	if(Main){auto* S=Main->AddChildToCanvas(Root);S->SetAnchors(FAnchors(0,0,1,1));S->SetOffsets(FMargin(0));S->SetZOrder(40);}
	else Root->AddToViewport(40);
	MainCanvas=Main;
}
void UVirtualSensorToolWorkspaceSubsystem::AttachOwnedPanel(UVirtualSensorPanelWidgetBase* P)
{
	if(!P||P->GetToolWorkspace()!=this)return;EnsureRoot();if(!Canvas)return;
	if(P->GetParent()!=Canvas){PendingInitialLayouts.Add(P->GetToolRole());const auto Pin=P->TakeWidget();P->RemoveFromParent();auto* S=Canvas->AddChildToCanvas(P);S->SetZOrder(++Front);}
	P->SetVisibility(IsPanelOpen(P->GetToolRole())?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
	RestorePanel(P->GetToolRole());
}
void UVirtualSensorToolWorkspaceSubsystem::ApplyDefault(ESensorToolPanelRole R)
{
	auto* P=GetOwnedPanel(R);if(!P)return;
	FVector2D V=P->GetPanelLayoutViewport();if(V.X<320||V.Y<200)return;
	const bool Monitor=R==ESensorToolPanelRole::Monitor;
	FVector2D Size=Monitor?FVector2D(FMath::Min(1100.0,V.X*.65),FMath::Min(700.0,V.Y-120)):R==ESensorToolPanelRole::Data?FVector2D(760,580):R==ESensorToolPanelRole::Settings?FVector2D(460,640):FVector2D(580,520);
	if(R==ESensorToolPanelRole::SlabCharts)Size=FVector2D(780,820);
	if(R==ESensorToolPanelRole::SlabProgress)Size=FVector2D(520,420);
	const bool ScenarioPanel=R==ESensorToolPanelRole::SlabProgress||R==ESensorToolPanelRole::Replay;
	if(ScenarioPanel)Size=FVector2D(FMath::Max(360.0,(V.X-48)*.5),V.Y-ToolbarReservedTop-16);
	if(R==ESensorToolPanelRole::Placement)Size=FVector2D(760,900);
	Size.X=FMath::Min(Size.X,V.X-32);Size.Y=FMath::Min(Size.Y,V.Y-ToolbarReservedTop-16);
	P->SetPanelResizable(true);P->ResizeHandleSize=32;
	P->SetPanelResizeLimits(R==ESensorToolPanelRole::Placement?FVector2D(520,FMath::Max(600.0f,480*GetOwnedPanelFontScale())):R==ESensorToolPanelRole::SlabCharts?FVector2D(720,720):Monitor?FVector2D(480,300):FVector2D(360,280),FVector2D::ZeroVector);
	FVirtualSensorPanelUiState S;S.bHasSavedSize=true;S.ExpandedSize=Size;
	P->ApplyWorkspaceLayout(S);
	const double Offset=R==ESensorToolPanelRole::Placement?0:static_cast<int32>(R)*24;
	P->SetWorkspacePosition(ScenarioPanel?FVector2D(R==ESensorToolPanelRole::SlabProgress?16:32+Size.X,ToolbarReservedTop):Monitor?FVector2D(V.X-Size.X-16,ToolbarReservedTop):FVector2D(16+Offset,ToolbarReservedTop+Offset));
}
void UVirtualSensorToolWorkspaceSubsystem::RestorePanel(ESensorToolPanelRole R)
{
	auto* P=GetOwnedPanel(R);if(!P||!Preferences)return;
	if(!IsOwnedPanelLayoutReady(R)){PendingInitialLayouts.Add(R);return;}
	TGuardValue<bool> Guard(bApplying,true);
	ApplyDefault(R);
	if(const auto* S=Preferences->Panels.Find(R))if(S->Layout.bHasSavedPosition||S->Layout.bHasSavedSize)P->ApplyWorkspaceLayout(S->Layout);
	PendingInitialLayouts.Remove(R);
}
bool UVirtualSensorToolWorkspaceSubsystem::IsOwnedPanelLayoutReady(ESensorToolPanelRole R) const
{
	const auto* P=GetOwnedPanel(R);if(!P||!Canvas||P->GetParent()!=Canvas)return false;
	const FVector2D Size=Canvas->GetCachedGeometry().GetLocalSize();
	return FMath::IsFinite(Size.X)&&FMath::IsFinite(Size.Y)&&Size.X>=320&&Size.Y>=200;
}
void UVirtualSensorToolWorkspaceSubsystem::Save(){if(!bApplying&&Preferences&&GetWorld()->IsGameWorld())UGameplayStatics::SaveGameToSlot(Preferences,SlotName,0);}
void UVirtualSensorToolWorkspaceSubsystem::SavePanel(ESensorToolPanelRole R)
{
	if(bApplying||!Preferences||PendingInitialLayouts.Contains(R)||!IsOwnedPanelLayoutReady(R))return;auto* P=GetOwnedPanel(R);if(!P)return;
	const bool Open=IsPanelOpen(R);auto& S=Preferences->Panels.FindOrAdd(R);S.bOpen=Open;S.Layout=P->CaptureWorkspaceLayout();Save();
}
bool UVirtualSensorToolWorkspaceSubsystem::EnsureOwnedReplayPanel()
{
	if(GetOwnedPanel(ESensorToolPanelRole::Replay))return true;
	if(!GetWorld()||!GetWorld()->GetFirstPlayerController())return false;
	for(TActorIterator<ASlabScenarioReplayUiHostActor> It(GetWorld());It;++It){It->ShowReplayPanel();break;}
	if(!GetOwnedPanel(ESensorToolPanelRole::Replay))GetWorld()->SpawnActor<ASlabScenarioReplayUiHostActor>();
	return GetOwnedPanel(ESensorToolPanelRole::Replay)!=nullptr;
}
void UVirtualSensorToolWorkspaceSubsystem::SetPanelOpen(ESensorToolPanelRole R,bool Open)
{
	if(!Preferences)return;
	if(Open&&R==ESensorToolPanelRole::Placement&&!GetOwnedPanel(R))
	{
		if(PlacementHost.IsValid())PlacementHost->ShowPlacementPanel(PanelOpenMessage);
		else PanelOpenMessage=TEXT("배치 창을 열 수 없습니다: 센서 UI Host를 연결하세요.");
		return;
	}
	PanelOpenMessage.Reset();
	if(Open&&R==ESensorToolPanelRole::Replay&&!GetOwnedPanel(R))
	{
		EnsureOwnedReplayPanel();
	}
	if(Open&&(R==ESensorToolPanelRole::SlabCharts||R==ESensorToolPanelRole::SlabProgress)&&!GetOwnedPanel(R))
	{
		for(TActorIterator<ASlabSimulationUiHostActor> It(GetWorld());It;++It){It->ShowSimulationPanels();break;}
		if(!GetOwnedPanel(R))GetWorld()->SpawnActor<ASlabSimulationUiHostActor>();
	}
	auto& State=Preferences->Panels.FindOrAdd(R);State.bOpen=Open;
	if(auto* P=GetOwnedPanel(R))
	{
		if(!Open)if(auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(P))Settings->SetSensorManipulationEnabled(false);
		if(!Open)if(auto* Placement=Cast<UVirtualSensorPlacementPanelWidget>(P))Placement->EndPlacement();
		P->SetVisibility(Open?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
		if(Open)
		{
			// A hidden first-open panel might never have received NativeTick during startup.
			const bool HasSavedLayout=State.Layout.bHasSavedPosition||State.Layout.bHasSavedSize;
			if(PendingInitialLayouts.Contains(R)||!HasSavedLayout)RestorePanel(R);
			if(!PendingInitialLayouts.Contains(R))P->RefreshHostedPanelLayout();
			BringOwnedPanelToFront(R);
			if(R==ESensorToolPanelRole::Placement&&P->IsPanelCollapsed())P->TogglePanelCollapsed();
		}
		if(Open)if(auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(P))Settings->BindSensorManager(Coordinator.Get());
	}
	Save();
}
void UVirtualSensorToolWorkspaceSubsystem::BringOwnedPanelToFront(ESensorToolPanelRole R)
{
	auto* P=GetOwnedPanel(R);if(!P)return;auto* Slot=Cast<UCanvasPanelSlot>(P->Slot);if(!Slot)return;
	auto* GI=GetWorld()->GetGameInstance();auto* Dx=GI?GI->GetSubsystem<UDxWidgetSubsystem>():nullptr;
	if(!Dx){Slot->SetZOrder(++Front);return;}
	// Scoped registration uses DTCore ordering without transferring general close/mode ownership.
	const auto Existing=Dx->GetOpenWidgets();TArray<UDxWidget*> Temporary;
	for(const auto& Entry:Panels)if(Entry.Value&&!Existing.Contains(Entry.Value.Get())){Dx->RegisterWidget(Entry.Value);Temporary.Add(Entry.Value);}
	Dx->BringToFront(P);
	for(auto* Added:Temporary)Dx->UnregisterWidget(Added);
	// This canvas is nested under Main/Viewport; its toolbar has a reserved local Z domain.
	if(Slot->GetZOrder()>=10000)Slot->SetZOrder(++Front);
	else Front=FMath::Max(Front,Slot->GetZOrder());
}
void UVirtualSensorToolWorkspaceSubsystem::ResetOwnedPanelLayout(ESensorToolPanelRole R)
{
	if(!Preferences)return;const bool Open=IsPanelOpen(R);Preferences->Panels.Remove(R);
	Preferences->Panels.FindOrAdd(R).bOpen=Open;RestorePanel(R);SavePanel(R);
}
void UVirtualSensorToolWorkspaceSubsystem::ResetOwnedWorkspaceLayout()
{
	if(!Preferences)return;Preferences->Panels.Reset();SetOwnedPanelFontScale(1);
	for(int32 I=0;I<7;++I){auto R=static_cast<ESensorToolPanelRole>(I);ResetOwnedPanelLayout(R);SetPanelOpen(R,R==ESensorToolPanelRole::Monitor||R==ESensorToolPanelRole::Placement);}Save();
}
void UVirtualSensorToolWorkspaceSubsystem::SetOwnedPanelFontScale(float S)
{
	if(!Preferences||!FMath::IsFinite(S))return;Preferences->FontScale=FMath::Clamp(S,.85f,1.5f);
	for(auto& P:Panels)if(P.Value)P.Value->ApplySensorToolFontScale(Preferences->FontScale);Save();
	if(auto* Placement=GetOwnedPanel(ESensorToolPanelRole::Placement))
	{
		// The owned panel base already scales minimum width for its header font.
		Placement->SetPanelResizeLimits(FVector2D(520,FMath::Max(600.0f,480*Preferences->FontScale)),FVector2D::ZeroVector);
		Placement->SetPanelExpandedSize(Placement->GetPanelExpandedSize(),false);Placement->RefreshHostedPanelLayout();
	}
	if(Toolbar)Toolbar->InvalidateLayoutAndVolatility();
}
void UVirtualSensorToolWorkspaceSubsystem::UnregisterPanel(UVirtualSensorPanelWidgetBase* P)
{
	if(P&&GetOwnedPanel(P->GetToolRole())==P)
	{
		if(auto* GI=GetWorld()->GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>())Dx->UnregisterExternalInputBlocker(P);
		SavePanel(P->GetToolRole());Panels.Remove(P->GetToolRole());PendingInitialLayouts.Remove(P->GetToolRole());P->SetToolWorkspace(nullptr,P->GetToolRole());
	}
}
void UVirtualSensorToolWorkspaceSubsystem::Tick(float D)
{
	if(auto* Monitor=Cast<UVirtualSensorMonitorPanelWidget>(GetOwnedPanel(ESensorToolPanelRole::Monitor)))
		Monitor->PumpPendingCaptureWork();
	PollTime+=D;if(PollTime<.2f||Panels.IsEmpty())return;PollTime=0;EnsureRoot();RefreshHosting();
	SynchronizeOwnedSelection();
	if(!Canvas)return;const FVector2D Size=Canvas->GetCachedGeometry().GetLocalSize();
	if(Size.X>=320&&Size.Y>=200)
	{
		const bool SizeChanged=!Size.Equals(LastCanvasSize,1);
		const float DesiredTop=CalculateToolbarReservedTop(Toolbar->GetDesiredSize().Y,Size.Y);
		const bool ToolbarChanged=!FMath::IsNearlyEqual(ToolbarReservedTop,DesiredTop,1.0f);
		if(SizeChanged||ToolbarChanged)
		{
			LastCanvasSize=Size;ToolbarReservedTop=DesiredTop;
			if(auto* S=Cast<UCanvasPanelSlot>(Toolbar->Slot))S->SetSize(FVector2D(Size.X-32,ToolbarReservedTop-16));
		}
		for(const auto& P:Panels)
		{
			if(SizeChanged||PendingInitialLayouts.Contains(P.Key))RestorePanel(P.Key);
			else if(ToolbarChanged&&P.Value){P.Value->SetPanelExpandedSize(P.Value->GetPanelExpandedSize(),false);P.Value->RefreshHostedPanelLayout();}
		}
	}
}
void UVirtualSensorToolWorkspaceSubsystem::SynchronizeOwnedSelection()
{
	if(PlacementInteraction)PlacementInteraction->SynchronizeSelection();
	if(auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(GetOwnedPanel(ESensorToolPanelRole::Settings))) Settings->SynchronizeWorkspaceSelection();
}
USensorPlacementInteraction* UVirtualSensorToolWorkspaceSubsystem::GetPlacementInteraction()
{
	if(!PlacementInteraction){PlacementInteraction=NewObject<USensorPlacementInteraction>(this);PlacementInteraction->BindCoordinator(GetCoordinator());}
	return PlacementInteraction;
}
TStatId UVirtualSensorToolWorkspaceSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorToolWorkspaceSubsystem,STATGROUP_Tickables);}
