#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/DxWidgetInfo.h"
#include "DxWidgetSubsystem.generated.h"

enum class EDxWidgetFlag : uint8;
class AInteractableActor;
class UDxWidget;
enum class EDxViewMode : uint8;

UCLASS()
class DTCORE_API UDxWidgetSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// Function
public:
	UDxWidgetSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	// DxLevelManager 통해서 UI 모드 변경
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SwitchUIMode(EDxViewMode NewMode);
	// 위젯 생성 및 열기
	UFUNCTION(BlueprintCallable, Category = "UI")
	UDxWidget* OpenWidget(AInteractableActor* InteractableActor);
	// 자식 위젯 생성 및 열기
	UFUNCTION(BlueprintCallable, Category = "UI")
	UDxWidget* OpenWidgetFromWidget(UDxWidget* DxWidget, EDxWidgetFlag TargetFlag);
	// 위젯 닫기
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseWidget(UDxWidget* CloseWidget);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ClosedWidgetFromWidget(UDxWidget* DxWidget, EDxWidgetFlag TargetFlag);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void BringToFront(UDxWidget* Widget);

	TArray<TObjectPtr<UDxWidget>> GetOpenWidgets();

	UDxWidget* GetMainWidget() const { return MainWidgetInstance; }

	// MainWidget에서 canvasPanel을 찾는 헬퍼 함수
	class UPanelWidget* GetAddWidgetPanel() const;

private:
	// 위젯 생성, 위치 설정, 리스트 추가 등 공통 로직을 처리
	UDxWidget* CreateWidgetInternal(TSubclassOf<UDxWidget> WidgetClass, const FVector2D& Position, AInteractableActor* OwnerActor, UDxWidget* ParentWidget, EDxWidgetFlag Flag);

protected:

	// Variable
public:
private:
	UPROPERTY()
	TObjectPtr<UDataTable> LevelDataTable;
	UPROPERTY()
	TObjectPtr<UDxWidget> MainWidgetInstance;

	// 열린 위젯들을 관리하는 배열
	UPROPERTY()
	TArray<TObjectPtr<UDxWidget>> OpenWidgets;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDxWidget> MainWidgetClass;
};
