// CraneDataTypes.h (새 파일 생성)
#pragma once

#include "CoreMinimal.h"
#include "CraneDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FCranePositionData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float TrolleyPosition = 0.f; // 트롤리 위치

	UPROPERTY(BlueprintReadWrite)
	float HoistHeight = 0.f; // 호이스트 높이

	UPROPERTY(BlueprintReadWrite)
	float GantryPosition = 0.f; // 갠트리 위치

	// 필요한 필드 추가...
};

USTRUCT(BlueprintType)
struct FCraneStateData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString CraneId;

	UPROPERTY(BlueprintReadWrite)
	FCranePositionData Position;

	UPROPERTY(BlueprintReadWrite)
	FString OperationStatus;

	// 타임스탬프, 상태 등 추가 필드...
};