// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DxWidgetSubsystem.h"
#include "InteractableActor.generated.h"

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

	void Click();
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void OnCursorHover(UPrimitiveComponent* HoveredComponent);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void OnCursorUnhover();
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void HighlightActor(bool activate, const TArray<UPrimitiveComponent*> meshes, bool isError);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void HighlightSingleMesh(bool activate, UPrimitiveComponent* mesh, bool isError);
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "InteractableActor")
	TArray<UPrimitiveComponent*> GetActorAllMesh();

private:
	// 내부적으로 하이라이트 상태 관리
	void UpdateHighlight(UPrimitiveComponent* NewHoveredComponent);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TMap<FString, FWidgetInfo> WidgetMap; // 위젯 클래스와 위치 정보 저장
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	FString WidgetFlag; // 현재 사용할 위젯 플래그
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "InteractableActor")
	bool ShortcutHighlight = false;
	// 하이라이트 모드 설정 (에디터에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractableActor")
	EHighlightMode HighlightMode = EHighlightMode::WholeActor;

private:
	UPROPERTY()
	TArray<UPrimitiveComponent*> CurrentHighlightedMeshes;
	UPROPERTY()
	UPrimitiveComponent* LastHoveredComponent = nullptr;
	UPROPERTY()
	TArray<UPrimitiveComponent*> CachedMeshes;

protected:

};
