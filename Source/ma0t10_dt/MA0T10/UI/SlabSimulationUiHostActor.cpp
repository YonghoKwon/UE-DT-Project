#include "SlabSimulationUiHostActor.h"
#include "SlabChartsPanelWidget.h"
#include "SlabProgressPanelWidget.h"
#include "VirtualSensorPanelHostComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

ASlabSimulationUiHostActor::ASlabSimulationUiHostActor()
{ PrimaryActorTick.bCanEverTick = false; PanelHost = CreateDefaultSubobject<UVirtualSensorPanelHostComponent>(TEXT("SlabPanelHost")); }
void ASlabSimulationUiHostActor::BeginPlay() { Super::BeginPlay(); bEnding=false; InitializationAttempts=0; ShowSimulationPanels(); }
void ASlabSimulationUiHostActor::BindSlabActor(ASlabActor* InSlab)
{
	SlabActor = IsValid(InSlab)&&InSlab->GetWorld()==GetWorld()?InSlab:nullptr;
	if (ChartsWidget) ChartsWidget->BindSlabActor(SlabActor);
	if (ProgressWidget) ProgressWidget->BindSlabActor(SlabActor);
}
void ASlabSimulationUiHostActor::ShowSimulationPanels()
{
	if (bEnding||!GetWorld()||GetNetMode()==NM_DedicatedServer) return;
	if (!GetWorld()->GetFirstPlayerController())
	{
		if(++InitializationAttempts<=40)
		{ InitializationMessage=TEXT("PlayerController 준비 대기 중 (최대 10초)"); GetWorldTimerManager().SetTimer(InitializationRetry,this,&ASlabSimulationUiHostActor::ShowSimulationPanels,.25f,false); }
		else { GetWorldTimerManager().ClearTimer(InitializationRetry); InitializationMessage=TEXT("PlayerController가 없어 Slab UI를 만들지 못했습니다. 준비 후 ShowSimulationPanels를 호출하세요."); }
		return;
	}
	GetWorldTimerManager().ClearTimer(InitializationRetry);
	if(SlabActor&&(!IsValid(SlabActor)||SlabActor->GetWorld()!=GetWorld())) SlabActor=nullptr;
	if (!SlabActor)
	{
		ASlabActor* Only = nullptr;
		for (TActorIterator<ASlabActor> It(GetWorld()); It; ++It) { if (Only) { Only = nullptr; break; } Only = *It; }
		SlabActor = Only; // Ambiguous ownership is never silently resolved to another Slab.
	}
	auto* Workspace = GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	if (!Workspace) return;
	if (!ChartsWidget && !Workspace->GetOwnedPanel(ESensorToolPanelRole::SlabCharts))
	{
		auto Class = ChartsWidgetClass;
		if (!Class) Class = LoadClass<USlabChartsPanelWidget>(nullptr, TEXT("/Game/MA0T10/UI/WBP_SlabChartsPanel.WBP_SlabChartsPanel_C"));
		if (!Class) Class = USlabChartsPanelWidget::StaticClass();
		ChartsWidget = CreateWidget<USlabChartsPanelWidget>(GetWorld(), Class);
		if (Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts, ChartsWidget))
		{
			ChartsWidget->SetPanelPersistenceKey(TEXT("SlabCharts")); ChartsWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(780, 820));
			ChartsWidget->SetPanelResizable(true); ChartsWidget->ResizeHandleSize = 32; ChartsWidget->SetPanelResizeLimits(FVector2D(720, 720), FVector2D::ZeroVector);
			PanelHost->RegisterPanel(ChartsWidget, 50);
		}
	}
	if (!ProgressWidget && !Workspace->GetOwnedPanel(ESensorToolPanelRole::SlabProgress))
	{
		auto Class = ProgressWidgetClass;
		if (!Class) Class = LoadClass<USlabProgressPanelWidget>(nullptr, TEXT("/Game/MA0T10/UI/WBP_SlabProgressPanel.WBP_SlabProgressPanel_C"));
		if (!Class) Class = USlabProgressPanelWidget::StaticClass();
		ProgressWidget = CreateWidget<USlabProgressPanelWidget>(GetWorld(), Class);
		if (Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress, ProgressWidget))
		{
			ProgressWidget->SetPanelPersistenceKey(TEXT("SlabProgress")); ProgressWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(520, 420));
			ProgressWidget->SetPanelResizable(true); ProgressWidget->ResizeHandleSize = 32; ProgressWidget->SetPanelResizeLimits(FVector2D(360, 280), FVector2D::ZeroVector);
			PanelHost->RegisterPanel(ProgressWidget, 51);
		}
	}
	BindSlabActor(SlabActor);
	InitializationMessage=SlabActor?TEXT("Slab UI 연결 완료"):TEXT("Slab를 하나로 결정할 수 없습니다. Host의 SlabActor를 직접 연결하세요.");
}
void ASlabSimulationUiHostActor::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding=true; if(GetWorld()) GetWorldTimerManager().ClearTimer(InitializationRetry);
	PanelHost->UnregisterAllPanels();
	if (ChartsWidget) ChartsWidget->RemoveFromParent(); if (ProgressWidget) ProgressWidget->RemoveFromParent();
	ChartsWidget = nullptr; ProgressWidget = nullptr; Super::EndPlay(Reason);
}
