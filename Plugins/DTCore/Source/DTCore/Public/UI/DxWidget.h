#pragma once

#include "CoreMinimal.h"
#include "DxWidgetThemeData.h"
#include "Blueprint/UserWidget.h"
#include "InteractableActor/InteractableActor.h"
#include "Player/DxPlayerBase.h"
#include "DxWidget.generated.h"

class UDxWidgetThemeData;
class UImage;
class UTextBlock;
class UButton;

UENUM(BlueprintType)
enum class EPlayerViewType : uint8
{
	TopView UMETA(DisplayName = "TopView"),
	FreeView UMETA(DisplayName = "FreeView"),
};
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
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DxWidget")
	void OpenWidgetAddLogic();

	UFUNCTION(BlueprintCallable, Category = "DxWidget")
	void CloseWidget();
	void CloseWidgetAddLogic_Implementation();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DxWidget")
	void CloseWidgetAddLogic();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DxWidget")
	void RetryWidget();
	virtual void RetryWidget_Implementation();


	UFUNCTION(BlueprintCallable, Category = "DxWidget|ChildWidget")
	UDxWidget* OpenChildWidget(EDxWidgetFlag InChildFlag);
	UFUNCTION(BlueprintCallable, Category = "DxWidget|ChildWidget")
	void CloseChildWidget(EDxWidgetFlag InChildFlag);

	UFUNCTION(BlueprintCallable, Category = "DxWidget")
	void SetParentWidget(UDxWidget* InParentWidget);
	UFUNCTION(BlueprintCallable, Category = "DxWidget")
	void AddChildWidget(UDxWidget* InChildWidget);
	UFUNCTION(BlueprintCallable, Category = "DxWidget")
	void RemoveChildWidget(UDxWidget* InChildWidget);


protected:
	// Player) 연속 동작 시작/중지 (자식 클래스에서 사용)
	void StartContinuousTimer();
	void StopContinuousTimer();

	// Player) 동작 업데이트 함수
	virtual void ContinuousUpdate();

	// Player 변경 이벤트 처리 (자식 클래스에서 오버라이드 가능)
	virtual void OnPlayerChanged(ADxPlayerBase* NewPlayer);

	// 테마 색상을 실제 위젯 컴포넌트에 적용하는 함수
	UFUNCTION(BlueprintNativeEvent, Category = "DxWidget|Theme")
	void ApplyTheme();
	virtual void ApplyTheme_Implementation(); // C++ 기본 구현부

private:
	// Player 변경 감지
	void BindToPlayerController();
	void UnbindFromPlayerController();

	UFUNCTION()
	void HandlePawnChanged(APawn* NewPawn);

	UFUNCTION()
	void OnCloseClicked();
	UFUNCTION()
	void OnRetryClicked();
	// Variable
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<ADxPlayerBase> Player;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<class ADxPlayerControllerBase> PlayerController;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Actor")
	AInteractableActor* MyActor;

	UPROPERTY(meta = (BindWidgetOptional), VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget|Theme")
	TObjectPtr<UImage> Img_TitleBg;
	UPROPERTY(meta = (BindWidgetOptional), VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget|Theme")
	TObjectPtr<UImage> Img_BodyBg;
	UPROPERTY(meta = (BindWidgetOptional), VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget|Theme")
	TObjectPtr<UTextBlock> Txt_Title;

	UPROPERTY(meta = (BindWidgetOptional), VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget")
	TObjectPtr<UButton> Btn_Close;
	UPROPERTY(meta = (BindWidgetOptional), VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget")
	TObjectPtr<UButton> Btn_Retry;

	// 직접 맵을 정의하는 대신 에셋을 참조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<UDxWidgetConfigData> WidgetConfig;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	EDxWidgetFlag WidgetFlag = EDxWidgetFlag::None;

	// 나를 호출한 부모 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget|Hierarchy")
	TWeakObjectPtr<UDxWidget> ParentWidget;

	// 내가 호출한 자식 위젯들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DxWidget|Hierarchy")
	TArray<TObjectPtr<UDxWidget>> ChildWidgets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget|Theme")
	TObjectPtr<UDxWidgetThemeData> ThemeData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget|Theme")
	EDxWidgetStyleType CurrentStyleType = EDxWidgetStyleType::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget|Runtime")
	FVector2D SpawnPosition = FVector2D::ZeroVector;
protected:
	UPROPERTY()
	FTimerHandle ContinuousTimerHandle;

private:
};
