#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "VirtualSensorToolWorkspaceSubsystem.generated.h"
class UVirtualSensorPanelWidgetBase;
class UUserWidget;
class UCanvasPanel;
class AVirtualSensorCoordinator;
class AVirtualSensorActorBase;
class AVirtualSensorUiHostActor;
class USensorPlacementInteraction;
class USensorMapCheckWidget;
class UAgentTestSetWidget;

UENUM(BlueprintType)
enum class ESensorToolPanelRole : uint8 { Monitor, Settings, Data, Replay, SlabCharts, SlabProgress, Placement };
USTRUCT()
struct FSensorToolWorkspacePanelState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FVirtualSensorPanelUiState Layout;
	UPROPERTY(SaveGame) bool bOpen=false;
};
UCLASS()
class MA0T10_DT_API USensorToolWorkspaceSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version=1;
	UPROPERTY(SaveGame) float FontScale=1;
	UPROPERTY(SaveGame) TMap<ESensorToolPanelRole,FSensorToolWorkspacePanelState> Panels;
};
/** Opt-in only. No discovery of arbitrary widgets or mutation of other developers' UI state. */
UCLASS()
class MA0T10_DT_API UVirtualSensorToolWorkspaceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float Delta) override;
	virtual TStatId GetStatId() const override;
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorWorkspace") bool RegisterOwnedPanel(ESensorToolPanelRole Role,UVirtualSensorPanelWidgetBase* Panel);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorWorkspace") void SetPanelOpen(ESensorToolPanelRole Role,bool bOpen);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorWorkspace") void BringOwnedPanelToFront(ESensorToolPanelRole Role);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorWorkspace") void ResetOwnedPanelLayout(ESensorToolPanelRole Role);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorWorkspace") void ResetOwnedWorkspaceLayout();
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorWorkspace") void SetOwnedPanelFontScale(float Scale);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorWorkspace") float GetOwnedPanelFontScale() const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorWorkspace") bool IsPanelOpen(ESensorToolPanelRole Role) const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorWorkspace") UVirtualSensorPanelWidgetBase* GetOwnedPanel(ESensorToolPanelRole Role) const;
	/** Prepare the existing replay owner without opening its window or changing output options. */
	bool EnsureOwnedReplayPanel();
	bool OpenMapCheck();
	bool OpenTestSet();
	UAgentTestSetWidget* GetTestSetWidget() const {return TestSetWidget;}
	USensorMapCheckWidget* GetMapCheckWidget() const { return MapCheckWidget; }
	void SetCoordinator(AVirtualSensorCoordinator* InCoordinator);
	bool RegisterPlacementHost(AVirtualSensorUiHostActor* Host);
	void UnregisterPlacementHost(AVirtualSensorUiHostActor* Host);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorWorkspace") FString GetPanelOpenMessage() const {return PanelOpenMessage;}
	AVirtualSensorCoordinator* GetCoordinator() const;
	USensorPlacementInteraction* GetPlacementInteraction();
	bool HasPlacementInteraction() const { return PlacementInteraction!=nullptr; }
	void AttachOwnedPanel(UVirtualSensorPanelWidgetBase* Panel);
	void SavePanel(ESensorToolPanelRole Role);
	void RestorePanel(ESensorToolPanelRole Role);
	void UnregisterPanel(UVirtualSensorPanelWidgetBase* Panel);
	void SynchronizeOwnedSelection();
	/** Startup layout must use the actually arranged owned Canvas, never a provisional viewport. */
	bool IsOwnedPanelLayoutReady(ESensorToolPanelRole Role) const;
	bool OwnsToolbar(const UUserWidget* Widget) const { return Toolbar==Widget; }
	UUserWidget* GetOwnedNavigation() const { return Toolbar; }
	float GetToolbarReservedTop() const { return ToolbarReservedTop; }
	static float CalculateToolbarReservedTop(float DesiredHeight,float ViewportHeight);
	static const FString SlotName;
	bool IsSidebarMode() const { return bSidebarMode; }
	void SetSidebarMode(bool bEnabled);
	void CollapseSidebar();
	ESensorToolPanelRole GetSidebarRole() const { return SidebarRole; }
	bool IsSidebarExpanded() const { return bSidebarExpanded; }
	float GetSidebarWidth() const;
	bool IsSidebarWide() const { return bSidebarWide; }
	void ToggleSidebarWidth();
	/** Transient screen navigation only; never serialized in Workspace v1. */
	void ClearPanelBackTarget(ESensorToolPanelRole Role);
	void SetPanelBackToTools(ESensorToolPanelRole Role);
	FText GetPanelBackLabel(ESensorToolPanelRole Role) const;
	bool CanNavigatePanelBack(ESensorToolPanelRole Role) const;
	bool NavigatePanelBack(ESensorToolPanelRole Role);
	float GetOwnedPixelScale() const;
	void RefreshDockedLayout();
	/** UXR-14: user-resized preview card rect while docked (session only; ⋯ 초기화 clears it). */
	void SetDockedCardRect(FVector2D Position,FVector2D Size);
	FVector2D GetDockedCardMinimum() const;
	FBox2D GetDockedCardBounds() const;
	bool HasCustomDockedCard() const { return bDockedCardCustom; }
	/** Actual arranged owned UI rectangles in viewport pixels, for L2 presentation. */
	TArray<FBox2D> GetOwnedDiagnosticExclusionRects() const;
private:
	void RegisterOwnedPointerRouting();
	void UnregisterOwnedPointerRouting();
	TSharedPtr<class IInputProcessor> OwnedPointerRouter;
	float MeasureOwnedPixelScale() const;
	bool bSidebarMode=true,bSidebarExpanded=true;
	bool bSidebarWide=false,bNavigatingBack=false;
	bool bDockedCardCustom=false;FVector2D DockedCardPosition=FVector2D::ZeroVector,DockedCardSize=FVector2D::ZeroVector;
	void ClampDockedCard(FVector2D& Position,FVector2D& Size) const;
	struct FPanelBackTarget
	{
		ESensorToolPanelRole Role=ESensorToolPanelRole::Placement;
		bool bTools=false;
		FText Label;
		TSharedPtr<FPanelBackTarget> ParentBack;
		int32 Depth=0;
	};
	TMap<ESensorToolPanelRole,FPanelBackTarget> PanelBackTargets;
	ESensorToolPanelRole SidebarRole=ESensorToolPanelRole::Placement;
	TSet<ESensorToolPanelRole> SidebarOpen={ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Placement};
	void RefreshOwnedPickMarkers();
	TArray<TWeakObjectPtr<AVirtualSensorActorBase>> OwnedPickTargets;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabWorkspaceDeferredLayoutTest;
	friend class FSensorWorkspacePlacementEntryTest;
	friend class FSensorWorkflowToolbarTest;
	friend class FSensorVocabularyTest;
	friend class FSensorWorkflowStatusTest;
	friend class FSidebarOwnershipTest;
	friend class FUXR10NavigationTest;
#endif
	UPROPERTY(Transient) TObjectPtr<USensorToolWorkspaceSaveGame> Preferences;
	UPROPERTY(Transient) TMap<ESensorToolPanelRole,TObjectPtr<UVirtualSensorPanelWidgetBase>> Panels;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Root;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Toolbar;
	UPROPERTY(Transient) TObjectPtr<USensorMapCheckWidget> MapCheckWidget;
	UPROPERTY(Transient) TObjectPtr<UAgentTestSetWidget> TestSetWidget;
	FString LastOpenedTestSet;
	UPROPERTY(Transient) TObjectPtr<USensorPlacementInteraction> PlacementInteraction;
	TWeakObjectPtr<UCanvasPanel> MainCanvas;
	TWeakObjectPtr<AVirtualSensorCoordinator> Coordinator;
	TWeakObjectPtr<AVirtualSensorUiHostActor> PlacementHost;
	FString PanelOpenMessage;
	FVector2D LastCanvasSize=FVector2D::ZeroVector;
	float PollTime=0;
	float LastOwnedPixelScale=0;
	float ToolbarReservedTop=64;
	int32 Front=10;
	bool bApplying=false;
	TSet<ESensorToolPanelRole> PendingInitialLayouts;
	void EnsureRoot();
	void RefreshHosting();
	void ApplyDefault(ESensorToolPanelRole Role);
	void Save();
};
