#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"

UVirtualSensorPanelHostComponent::UVirtualSensorPanelHostComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorPanelHostComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MainWidgetPollTimer,
			this,
			&UVirtualSensorPanelHostComponent::RefreshHosting,
			FMath::Max(0.1f, MainWidgetPollInterval),
			true,
			0.0f);
	}
}

void UVirtualSensorPanelHostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(MainWidgetPollTimer);
	UnregisterAllPanels();
	Super::EndPlay(EndPlayReason);
}

void UVirtualSensorPanelHostComponent::RegisterPanel(UVirtualSensorPanelWidgetBase* Panel, int32 DefaultZOrder)
{
	if (!Panel) return;
	for (FVirtualSensorHostedPanel& Entry : HostedPanels)
	{
		if (Entry.Panel == Panel) return;
	}
	FVirtualSensorHostedPanel& Entry = HostedPanels.AddDefaulted_GetRef();
	Entry.Panel = Panel;
	Entry.DefaultZOrder = DefaultZOrder;
	Panel->SetPanelHostComponent(this);
	AttachPanel(Entry, ResolveMainCanvas());
}

void UVirtualSensorPanelHostComponent::UnregisterAllPanels()
{
	for (FVirtualSensorHostedPanel& Entry : HostedPanels)
	{
		if (Entry.Panel)
		{
			Entry.Panel->SetPanelHostComponent(nullptr);
			Entry.Panel->RemoveFromParent();
		}
	}
	HostedPanels.Reset();
	CurrentMainCanvas = nullptr;
}

void UVirtualSensorPanelHostComponent::RefreshHosting()
{
	UCanvasPanel* TargetCanvas = ResolveMainCanvas();
	if (TargetCanvas == CurrentMainCanvas) return;
	CurrentMainCanvas = TargetCanvas;
	for (FVirtualSensorHostedPanel& Entry : HostedPanels) AttachPanel(Entry, TargetCanvas);
}

void UVirtualSensorPanelHostComponent::BringPanelToFront(UVirtualSensorPanelWidgetBase* Panel)
{
	if (!Panel) return;
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
	{
		CanvasSlot->SetZOrder(++FrontZOrder);
	}
	else
	{
		// Reparenting a viewport widget during MouseButtonDown invalidates the
		// cached Slate widget before NativeOnMouseButtonDown can capture it. Keep
		// the panel attached so the following move/up events complete the drag.
		++FrontZOrder;
	}
}

UCanvasPanel* UVirtualSensorPanelHostComponent::ResolveMainCanvas() const
{
	if (!bPreferMainWidgetPanel) return nullptr;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UDxWidgetSubsystem* WidgetSubsystem = GameInstance ? GameInstance->GetSubsystem<UDxWidgetSubsystem>() : nullptr;
	return WidgetSubsystem ? Cast<UCanvasPanel>(WidgetSubsystem->GetAddWidgetPanel()) : nullptr;
}

void UVirtualSensorPanelHostComponent::AttachPanel(FVirtualSensorHostedPanel& Entry, UCanvasPanel* TargetCanvas)
{
	UVirtualSensorPanelWidgetBase* Panel = Entry.Panel;
	if (!Panel) return;
	Panel->RemoveFromParent();
	if (TargetCanvas)
	{
		UCanvasPanelSlot* Slot = TargetCanvas->AddChildToCanvas(Panel);
		if (Slot) Slot->SetZOrder(Entry.DefaultZOrder);
	}
	else if (bAllowViewportFallback)
	{
		Panel->AddToViewport(Entry.DefaultZOrder);
	}
	Panel->RefreshHostedPanelLayout();
}
