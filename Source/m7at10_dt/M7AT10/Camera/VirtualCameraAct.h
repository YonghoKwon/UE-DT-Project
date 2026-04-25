// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualCameraAct.generated.h"

class UVirtualCameraComp;
class USensorDataPublisherComp;

UCLASS()
class M7AT10_DT_API AVirtualCameraAct : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVirtualCameraAct();

	// 뷰포트에서 확인하고 조작할 수 있도록 루트 컴포넌트로 사용할 가상 카메라 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UVirtualCameraComp> VirtualCameraComp;

	// 캡처 데이터를 외부 시스템으로 전달하는 전송 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USensorDataPublisherComp> SensorPublisherComp;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
