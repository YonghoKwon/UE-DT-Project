#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolAppearance.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"

const FString UVirtualSensorToolWorkspaceSubsystem::SlotName=TEXT("MA0T10_SensorToolWorkspace_v1");
void UVirtualSensorToolWorkspaceSubsystem::SetCoordinator(AVirtualSensorCoordinator* C){Coordinator=C;}
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
	for(auto& P:Panels)if(P.Value)P.Value->SetToolWorkspace(nullptr,P.Key);
	if(Root)Root->RemoveFromParent();Panels.Reset();Root=nullptr;Canvas=nullptr;Toolbar=nullptr;Super::Deinitialize();
}
float UVirtualSensorToolWorkspaceSubsystem::GetOwnedPanelFontScale() const { return Preferences?Preferences->FontScale:1; }
UVirtualSensorPanelWidgetBase* UVirtualSensorToolWorkspaceSubsystem::GetOwnedPanel(ESensorToolPanelRole R) const {const auto* P=Panels.Find(R);return P?P->Get():nullptr;}
bool UVirtualSensorToolWorkspaceSubsystem::IsPanelOpen(ESensorToolPanelRole R) const {const auto* S=Preferences?Preferences->Panels.Find(R):nullptr;return S?S->bOpen:R==ESensorToolPanelRole::Monitor;}
bool UVirtualSensorToolWorkspaceSubsystem::RegisterOwnedPanel(ESensorToolPanelRole R,UVirtualSensorPanelWidgetBase* P)
{
	if(!P||P->GetWorld()!=GetWorld()||!Preferences)return false;
	const bool Allowed=(R==ESensorToolPanelRole::Monitor&&P->IsA<UVirtualSensorMonitorPanelWidget>())||(R==ESensorToolPanelRole::Settings&&P->IsA<UVirtualSensorSettingsPanelWidget>())||(R==ESensorToolPanelRole::Data&&P->IsA<UVirtualSensorCaptureExportPanelWidget>())||(R==ESensorToolPanelRole::Replay&&P->IsA<USlabScenarioReplayPanelWidget>());
	if(!Allowed)return false;
	if(auto* Existing=GetOwnedPanel(R))return Existing==P;
	Panels.Add(R,P);P->SetToolWorkspace(this,R);P->ApplySensorToolFontScale(GetOwnedPanelFontScale());return true;
}
void UVirtualSensorToolWorkspaceSubsystem::EnsureRoot()
{
	if(Root||!GetWorld()->GetFirstPlayerController())return;
	Root=CreateWidget<UUserWidget>(GetWorld(),USensorToolWorkspaceRootWidget::StaticClass());
	Canvas=Root->WidgetTree->ConstructWidget<UCanvasPanel>();Root->WidgetTree->RootWidget=Canvas;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Toolbar=CreateWidget<UUserWidget>(GetWorld(),USensorToolToolbarWidget::StaticClass());
	auto* S=Canvas->AddChildToCanvas(Toolbar);S->SetPosition(FVector2D(16,8));S->SetSize(FVector2D(1000,88));S->SetZOrder(10000);
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
	if(P->GetParent()!=Canvas){const auto Pin=P->TakeWidget();P->RemoveFromParent();auto* S=Canvas->AddChildToCanvas(P);S->SetZOrder(++Front);}
	P->SetVisibility(IsPanelOpen(P->GetToolRole())?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
	RestorePanel(P->GetToolRole());
}
void UVirtualSensorToolWorkspaceSubsystem::ApplyDefault(ESensorToolPanelRole R)
{
	auto* P=GetOwnedPanel(R);if(!P)return;
	FVector2D V=P->GetPanelLayoutViewport();if(V.X<320||V.Y<200)return;
	const bool Monitor=R==ESensorToolPanelRole::Monitor;
	FVector2D Size=Monitor?FVector2D(FMath::Min(1100.0,V.X*.65),FMath::Min(700.0,V.Y-120)):R==ESensorToolPanelRole::Data?FVector2D(760,580):R==ESensorToolPanelRole::Settings?FVector2D(460,640):FVector2D(580,520);
	Size.X=FMath::Min(Size.X,V.X-32);Size.Y=FMath::Min(Size.Y,V.Y-120);
	P->SetPanelResizable(true);P->ResizeHandleSize=32;
	P->SetPanelResizeLimits(Monitor?FVector2D(480,300):FVector2D(360,280),FVector2D::ZeroVector);
	FVirtualSensorPanelUiState S;S.bHasSavedSize=true;S.ExpandedSize=Size;
	P->ApplyWorkspaceLayout(S);
	const double Offset=static_cast<int32>(R)*24;
	P->SetWorkspacePosition(Monitor?FVector2D(V.X-Size.X-16,104):FVector2D(16+Offset,104+Offset));
}
void UVirtualSensorToolWorkspaceSubsystem::RestorePanel(ESensorToolPanelRole R)
{
	auto* P=GetOwnedPanel(R);if(!P||!Preferences)return;TGuardValue<bool> Guard(bApplying,true);
	ApplyDefault(R);
	if(const auto* S=Preferences->Panels.Find(R))if(S->Layout.bHasSavedPosition||S->Layout.bHasSavedSize)P->ApplyWorkspaceLayout(S->Layout);
}
void UVirtualSensorToolWorkspaceSubsystem::Save(){if(!bApplying&&Preferences&&GetWorld()->IsGameWorld())UGameplayStatics::SaveGameToSlot(Preferences,SlotName,0);}
void UVirtualSensorToolWorkspaceSubsystem::SavePanel(ESensorToolPanelRole R)
{
	if(bApplying||!Preferences)return;auto* P=GetOwnedPanel(R);if(!P)return;
	const bool Open=IsPanelOpen(R);auto& S=Preferences->Panels.FindOrAdd(R);S.bOpen=Open;S.Layout=P->CaptureWorkspaceLayout();Save();
}
void UVirtualSensorToolWorkspaceSubsystem::SetPanelOpen(ESensorToolPanelRole R,bool Open)
{
	if(!Preferences)return;
	if(Open&&R==ESensorToolPanelRole::Replay&&!GetOwnedPanel(R))
	{
		for(TActorIterator<ASlabScenarioReplayUiHostActor> It(GetWorld());It;++It){It->ShowReplayPanel();break;}
		if(!GetOwnedPanel(R))GetWorld()->SpawnActor<ASlabScenarioReplayUiHostActor>();
	}
	auto& State=Preferences->Panels.FindOrAdd(R);State.bOpen=Open;
	if(auto* P=GetOwnedPanel(R))
	{
		if(!Open)if(auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(P))Settings->SetSensorManipulationEnabled(false);
		P->SetVisibility(Open?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
		if(Open){P->RefreshHostedPanelLayout();BringOwnedPanelToFront(R);}
		if(Open)if(auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(P))Settings->BindSensorManager(Coordinator.Get());
	}
	Save();
}
void UVirtualSensorToolWorkspaceSubsystem::BringOwnedPanelToFront(ESensorToolPanelRole R)
{ if(auto* P=GetOwnedPanel(R))if(auto* S=Cast<UCanvasPanelSlot>(P->Slot))S->SetZOrder(++Front); }
void UVirtualSensorToolWorkspaceSubsystem::ResetOwnedPanelLayout(ESensorToolPanelRole R)
{
	if(!Preferences)return;const bool Open=IsPanelOpen(R);Preferences->Panels.Remove(R);
	{TGuardValue<bool> Guard(bApplying,true);ApplyDefault(R);} Preferences->Panels.FindOrAdd(R).bOpen=Open;SavePanel(R);
}
void UVirtualSensorToolWorkspaceSubsystem::ResetOwnedWorkspaceLayout()
{
	if(!Preferences)return;Preferences->Panels.Reset();SetOwnedPanelFontScale(1);
	for(int32 I=0;I<4;++I){auto R=static_cast<ESensorToolPanelRole>(I);ResetOwnedPanelLayout(R);SetPanelOpen(R,R==ESensorToolPanelRole::Monitor);}Save();
}
void UVirtualSensorToolWorkspaceSubsystem::SetOwnedPanelFontScale(float S)
{
	if(!Preferences||!FMath::IsFinite(S))return;Preferences->FontScale=FMath::Clamp(S,.85f,1.5f);
	for(auto& P:Panels)if(P.Value)P.Value->ApplySensorToolFontScale(Preferences->FontScale);Save();
}
void UVirtualSensorToolWorkspaceSubsystem::UnregisterPanel(UVirtualSensorPanelWidgetBase* P)
{if(P&&GetOwnedPanel(P->GetToolRole())==P){SavePanel(P->GetToolRole());Panels.Remove(P->GetToolRole());P->SetToolWorkspace(nullptr,P->GetToolRole());}}
void UVirtualSensorToolWorkspaceSubsystem::Tick(float D)
{
	if(auto* Monitor=Cast<UVirtualSensorMonitorPanelWidget>(GetOwnedPanel(ESensorToolPanelRole::Monitor)))
		Monitor->PumpPendingCaptureWork();
	PollTime+=D;if(PollTime<.2f||Panels.IsEmpty())return;PollTime=0;EnsureRoot();RefreshHosting();
	SynchronizeOwnedSelection();
	if(!Canvas)return;const FVector2D Size=Canvas->GetCachedGeometry().GetLocalSize();
	if(Size.X>=320&&Size.Y>=200&&!Size.Equals(LastCanvasSize,1))
	{
		LastCanvasSize=Size;if(auto* S=Cast<UCanvasPanelSlot>(Toolbar->Slot))S->SetSize(FVector2D(Size.X-32,88));
		for(const auto& P:Panels)RestorePanel(P.Key);
	}
}
void UVirtualSensorToolWorkspaceSubsystem::SynchronizeOwnedSelection()
{
	if(auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(GetOwnedPanel(ESensorToolPanelRole::Settings))) Settings->SynchronizeWorkspaceSelection();
}
TStatId UVirtualSensorToolWorkspaceSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorToolWorkspaceSubsystem,STATGROUP_Tickables);}
