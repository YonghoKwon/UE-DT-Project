#include "SlabScenarioReplayUiHostActor.h"
#include "SlabScenarioReplayPanelWidget.h"
#include "VirtualSensorPanelHostComponent.h"
#include "GameFramework/PlayerController.h"
ASlabScenarioReplayUiHostActor::ASlabScenarioReplayUiHostActor()
{ PrimaryActorTick.bCanEverTick=false; PanelHost=CreateDefaultSubobject<UVirtualSensorPanelHostComponent>(TEXT("ReplayPanelHost")); }
void ASlabScenarioReplayUiHostActor::BeginPlay() { Super::BeginPlay(); ShowReplayPanel(); }
void ASlabScenarioReplayUiHostActor::ShowReplayPanel()
{
	if(ReplayWidget||!GetWorld()||!GetWorld()->GetFirstPlayerController()) return;
	auto Class=ReplayWidgetClass;
	if(!Class) Class=LoadClass<USlabScenarioReplayPanelWidget>(nullptr,TEXT("/Game/MA0T10/UI/WBP_SlabScenarioReplayPanel.WBP_SlabScenarioReplayPanel_C"));
	if(!Class) Class=USlabScenarioReplayPanelWidget::StaticClass();
	ReplayWidget=CreateWidget<USlabScenarioReplayPanelWidget>(GetWorld(),Class);
	GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->RegisterOwnedPanel(ESensorToolPanelRole::Replay,ReplayWidget);
	ReplayWidget->SetPanelPersistenceKey(TEXT("SlabScenarioReplay"));
	ReplayWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::RightCenter,FVector2D(600,540));
	ReplayWidget->SetPanelResizable(true); ReplayWidget->ResizeHandleSize=32;
	ReplayWidget->SetPanelResizeLimits(FVector2D(420,300),FVector2D::ZeroVector);
	PanelHost->RegisterPanel(ReplayWidget,40);
}
void ASlabScenarioReplayUiHostActor::EndPlay(const EEndPlayReason::Type Reason)
{ PanelHost->UnregisterAllPanels(); if(ReplayWidget) ReplayWidget->RemoveFromParent(); ReplayWidget=nullptr; Super::EndPlay(Reason); }
