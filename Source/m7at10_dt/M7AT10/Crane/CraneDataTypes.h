// CraneDataTypes.h (새 파일 생성)
#pragma once

#include "CoreMinimal.h"
#include "Core/DxDataType.h"
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

// Actor 구조체에 꼭 FDxDataBase를 상속받아야 함!!!
struct FCraneStateData : public FDxDataBase
{
	FString CraneId;
	FCranePositionData Position;
	FString OperationStatus;

	// 타임스탬프, 상태 등 추가 필드...

	virtual EDxDataType GetType() const override
	{
		return EDxDataType::CraneState;
	}
};