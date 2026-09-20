#include "SlabSimulationUiHostActor.h"
#include "SlabChartsPanelWidget.h"
#include "SlabProgressPanelWidget.h"
#include "VirtualSensorPanelHostComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

ASlabSimulationUiHostActor::ASlabSimulationUiHostActor()
{ PrimaryActorTick.bCanEverTick = false; PanelHost = CreateDefaultSubobject<UVirtualSensorPanelHostComponent>(TEXT("SlabPanelHost")); }
void ASlabSimulationUiHostActor::BeginPlay() { Super::BeginPlay(); ShowSimulationPanels(); }
void ASlabSimulationUiHostActor::BindSlabActor(ASlabActor* InSlab)
{
	SlabActor = InSlab;
	if (ChartsWidget) ChartsWidget->BindSlabActor(InSlab);
	if (ProgressWidget) ProgressWidget->BindSlabActor(InSlab);
}
void ASlabSimulationUiHostActor::ShowSimulationPanels()
{
	if (!GetWorld() || !GetWorld()->GetFirstPlayerController()) return;
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
			ChartsWidget->SetPanelPersistenceKey(TEXT("SlabCharts")); ChartsWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(720, 540));
			ChartsWidget->SetPanelResizable(true); ChartsWidget->ResizeHandleSize = 32; ChartsWidget->SetPanelResizeLimits(FVector2D(420, 320), FVector2D::ZeroVector);
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
}
void ASlabSimulationUiHostActor::EndPlay(const EEndPlayReason::Type Reason)
{
	PanelHost->UnregisterAllPanels();
	if (ChartsWidget) ChartsWidget->RemoveFromParent(); if (ProgressWidget) ProgressWidget->RemoveFromParent();
	ChartsWidget = nullptr; ProgressWidget = nullptr; Super::EndPlay(Reason);
}
