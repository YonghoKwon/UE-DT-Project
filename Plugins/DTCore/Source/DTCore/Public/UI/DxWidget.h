#pragma once

#include "CoreMinimal.h"
#include "DxWidgetConfigData.h"
#include "DxWidgetDataType.h"
#include "Blueprint/UserWidget.h"
#include "Player/DxPlayerBase.h"
#include "DxWidget.generated.h"

class AInteractableActor;
/**
 * 
 */
UCLASS()
class DTCORE_API UDxWidget : public UUserWidget
{
	GENERATED_BODY()

	// Function
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Widget")
	void OpenWidgetAddLogic();

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void CloseWidget();
protected:
	// Player) 연속 동작 시작/중지 (자식 클래스에서 사용)
	void StartContinuousTimer();
	void StopContinuousTimer();

	// Player) 동작 업데이트 함수
	virtual void ContinuousUpdate();

	// Player 변경 이벤트 처리 (자식 클래스에서 오버라이드 가능)
	virtual void OnPlayerChanged(ADxPlayerBase* NewPlayer);

private:
	// Player 변경 감지
	void BindToPlayerController();
	void UnbindFromPlayerController();

	UFUNCTION()
	void HandlePawnChanged(APawn* NewPawn);

	// Variable
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<ADxPlayerBase> Player;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<class ADxPlayerControllerBase> PlayerController;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Actor")
	AInteractableActor* MyActor;

	// 직접 맵을 정의하는 대신 에셋을 참조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<UDxWidgetConfigData> WidgetConfig;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	EDxWidgetFlag WidgetFlag = EDxWidgetFlag::None;
protected:
	UPROPERTY()
	FTimerHandle ContinuousTimerHandle;

private:
};
