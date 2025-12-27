#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxWidgetSubsystem.generated.h"

class AInteractableActor;
class UDxWidget;
enum class EDxViewMode : uint8;

// 위젯 정보를 담는 구조체
USTRUCT(BlueprintType)
struct FWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	TSubclassOf<UDxWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	FVector2D Position = FVector2D(1000.0f, 500.0f);
};
UCLASS()
class M7AT10_DT_API UDxWidgetSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
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
	// 위젯 닫기
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseWidget(UDxWidget* CloseWidget);
private:
	// MainWidget에서 canvasPanel을 찾는 헬퍼 함수
	class UPanelWidget* GetAddWidgetPanel() const;
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
