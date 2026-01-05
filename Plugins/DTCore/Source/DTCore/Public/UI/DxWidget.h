#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DxWidget.generated.h"

class AInteractableActor;
class ADxPlayerTest;
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
private:
protected:
	// Player) 연속 동작 시작/중지 (자식 클래스에서 사용)
	void StartContinuousTimer();
	void StopContinuousTimer();

	// Player) 동작 업데이트 함수
	virtual void ContinuousUpdate();

	// Variable
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<ADxPlayerTest> Player;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<class ADxPlayerControllerBase> PlayerController;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Actor")
	AInteractableActor* MyActor;
private:
protected:
	UPROPERTY()
	FTimerHandle ContinuousTimerHandle;
};
