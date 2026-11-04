#include "ma0t10_dt/MA0T10/UI/Main/Slab/SlabScenarioReplayUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetSlabScenarioReplayPanel.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/VirtualSensorPanelHostComp.h"
#include "GameFramework/PlayerController.h"
ASlabScenarioReplayUiHostActor::ASlabScenarioReplayUiHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PanelHostComp = CreateDefaultSubobject<UVirtualSensorPanelHostComp>(TEXT("ReplayPanelHost"));
}
void ASlabScenarioReplayUiHostActor::BeginPlay()
{
	Super::BeginPlay();
	ShowReplayPanel();
}
void ASlabScenarioReplayUiHostActor::ShowReplayPanel()
{
	if (ReplayWidget || !GetWorld() || !GetWorld()->GetFirstPlayerController())
		return;
	auto Class = ReplayWidgetClass;
	if (!Class)
		Class = LoadClass<UDxWidgetSlabScenarioReplayPanel>(nullptr, TEXT("/Game/MA0T10/UI/WBP_SlabScenarioReplayPanel.WBP_SlabScenarioReplayPanel_C"));
	if (!Class)
		Class = UDxWidgetSlabScenarioReplayPanel::StaticClass();
	ReplayWidget = CreateWidget<UDxWidgetSlabScenarioReplayPanel>(GetWorld(), Class);
	GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->RegisterOwnedPanel(ESensorToolPanelRole::Replay, ReplayWidget);
	ReplayWidget->SetPanelPersistenceKey(TEXT("SlabScenarioReplay"));
	ReplayWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::RightCenter, FVector2D(600, 540));
	ReplayWidget->SetPanelResizable(true);
	ReplayWidget->ResizeHandleSize = 32;
	ReplayWidget->SetPanelResizeLimits(FVector2D(420, 300), FVector2D::ZeroVector);
	PanelHostComp->RegisterPanel(ReplayWidget, 40);
}
void ASlabScenarioReplayUiHostActor::EndPlay(const EEndPlayReason::Type Reason)
{
	PanelHostComp->UnregisterAllPanels();
	if (ReplayWidget)
		ReplayWidget->RemoveFromParent();
	ReplayWidget = nullptr;
	Super::EndPlay(Reason);
}
