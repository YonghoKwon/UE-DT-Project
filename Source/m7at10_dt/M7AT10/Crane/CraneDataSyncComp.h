// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CraneDataTypes.h"
#include "ActorComponent/DataSyncCompBase.h"
#include "CraneDataSyncComp.generated.h"

// 데이터 변경을 알리기 위한 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCranePositionChanged, const FCranePositionData&, PositionData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraneStatusColorChanged, const FLinearColor&, NewColor);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UCraneDataSyncComp : public UDataSyncCompBase
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCraneDataSyncComp();

	// Function
public:
	// 메인 데이터 수신 함수
	UFUNCTION()
	void OnReceiveCraneStateData(const FCraneStateData& InData);
	// 부모 가상 함수 오버라이드
	virtual void OnReceiveData(EDxDataType DataType, const void* Data) override;
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintAssignable, Category = "Crane|Events")
	FOnCranePositionChanged OnCranePositionChanged;
	UPROPERTY(BlueprintAssignable, Category = "Crane|Events")
	FOnCraneStatusColorChanged OnCraneStatusColorChanged;
private:
protected:
};
