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

UENUM(BlueprintType)
enum class ESensorToolPanelRole : uint8 { Monitor, Settings, Data, Replay };
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
	void SetCoordinator(AVirtualSensorCoordinator* InCoordinator);
	AVirtualSensorCoordinator* GetCoordinator() const;
	void AttachOwnedPanel(UVirtualSensorPanelWidgetBase* Panel);
	void SavePanel(ESensorToolPanelRole Role);
	void RestorePanel(ESensorToolPanelRole Role);
	void UnregisterPanel(UVirtualSensorPanelWidgetBase* Panel);
	void SynchronizeOwnedSelection();
	static const FString SlotName;
private:
	UPROPERTY(Transient) TObjectPtr<USensorToolWorkspaceSaveGame> Preferences;
	UPROPERTY(Transient) TMap<ESensorToolPanelRole,TObjectPtr<UVirtualSensorPanelWidgetBase>> Panels;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Root;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Toolbar;
	TWeakObjectPtr<UCanvasPanel> MainCanvas;
	TWeakObjectPtr<AVirtualSensorCoordinator> Coordinator;
	FVector2D LastCanvasSize=FVector2D::ZeroVector;
	float PollTime=0;
	int32 Front=10;
	bool bApplying=false;
	void EnsureRoot();
	void RefreshHosting();
	void ApplyDefault(ESensorToolPanelRole Role);
	void Save();
};
