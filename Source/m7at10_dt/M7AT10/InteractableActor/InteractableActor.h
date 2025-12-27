// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "m7at10_dt/M7AT10/Core/DxWidgetSubsystem.h"
#include "m7at10_dt/M7AT10/UI/DxWidget.h"
#include "InteractableActor.generated.h"

// // 위젯 정보를 담는 구조체
// USTRUCT(BlueprintType)
// struct FWidgetInfo
// {
// 	GENERATED_BODY()
//
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
// 	TSubclassOf<UDxWidget> WidgetClass;
//
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
// 	FVector2D Position = FVector2D(1000.0f, 500.0f);
// };

// 하이라이트 모드
UENUM(BlueprintType)
enum class EHighlightMode : uint8
{
	WholeActor UMETA(DisplayName = "Whole Actor"),         // 액터 전체 하이라이트
	IndividualMesh UMETA(DisplayName = "Individual Mesh"), // 개별 메시 하이라이트
};

UCLASS()
class M7AT10_DT_API AInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AInteractableActor();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void Click();
	void Hover(TArray<UPrimitiveComponent*> mesh);
	void NotHover(TArray<UPrimitiveComponent*> mesh);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void HighlightActor(bool activate, TArray<UPrimitiveComponent*> mesh, bool isError);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	void HighlightSingleMesh(bool activate, UPrimitiveComponent* mesh, bool isError);
	UFUNCTION(BlueprintCallable, Category = "InteractableActor")
	TArray<UPrimitiveComponent*> GetActorAllMesh();


	// UFUNCTION()
	// void HighlightSingleMeh();
	//
	// UFUNCTION()
	// void HighlightMultipleMeshes(const TArray<UPrimitiveComponent*>& Meshes, bool bActivate, bool bIsError = false);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TMap<FString, FWidgetInfo> WidgetMap; // 위젯 클래스와 위치 정보 저장

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DxWidget")
	FString WidgetFlag; // 현재 사용할 위젯 플래그

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractableActor")
	bool ShortcutHighlight = false;

	// 하이라이트 모드 설정 (에디터에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractableActor")
	EHighlightMode HighlightMode = EHighlightMode::WholeActor;

	// UPROPERTY(EditAnywhere)
	// TArray<UPrimitiveComponent*> HighlightTagMeshes;
protected:
private:
};
