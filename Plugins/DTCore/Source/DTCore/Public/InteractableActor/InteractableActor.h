#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DxWidgetSubsystem.h"
#include "UI/DxWidgetDataType.h"
#include "InteractableActor.generated.h"

class UDxWidgetConfigData;
class ADxPlayerBase;
// 하이라이트 모드
UENUM(BlueprintType)
enum class EHighlightMode : uint8
{
	WholeActor UMETA(DisplayName = "Whole Actor"),         // 액터 전체 하이라이트
	IndividualMesh UMETA(DisplayName = "Individual Mesh")  // 개별 메시 하이라이트
};

UCLASS()
class DTCORE_API AInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	AInteractableActor();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	virtual void Click();
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	virtual void OnCursorHover(UPrimitiveComponent* HoveredComponent);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void OnCursorUnhover();
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	static void HighlightActor(bool activate, const TArray<UPrimitiveComponent*> meshes, bool isError);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void HighlightSingleMesh(bool activate, UPrimitiveComponent* mesh, bool isError);
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "InteractableActor")
	TArray<UPrimitiveComponent*> GetActorAllMesh();

	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void RefreshCachedMeshes();

protected:
	// Player 바인딩 관련 함수
	void BindToPlayer();
	void UnbindFromPlayer();

private:
	// 내부적으로 하이라이트 상태 관리
	void UpdateHighlight(UPrimitiveComponent* NewHoveredComponent);

protected:
	virtual void BeginPlay() override;

public:
	// 직접 맵을 정의하는 대신 에셋을 참조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<UDxWidgetConfigData> WidgetConfig;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	EDxWidgetFlag WidgetFlag = EDxWidgetFlag::None; // 현재 사용할 위젯 플래그

	// TODO: ShortcutHighlight 변수 사용하는지 확인 필요
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "InteractableActor")
	bool ShortcutHighlight = false;
	// 하이라이트 모드 설정 (에디터에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractableActor")
	EHighlightMode HighlightMode = EHighlightMode::WholeActor;

	// 액터 표시 이름 (위젯에서 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractableActor")
	FString DisplayName;

private:
	UPROPERTY()
	TArray<UPrimitiveComponent*> CurrentHighlightedMeshes;
	UPROPERTY()
	UPrimitiveComponent* LastHoveredComponent = nullptr;
	UPROPERTY()
	TArray<UPrimitiveComponent*> CachedMeshes;

protected:
	UPROPERTY()
	TObjectPtr<ADxPlayerBase> CachedPlayer;
};
