// CraneDataTypes.h (새 파일 생성)
#pragma once

#include "CoreMinimal.h"
#include "Core/DxDataType.h"
#include "CraneDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FCranePositionData
{
	GENERATED_BODY()

	// [수정] 본 구조에 맞는 데이터 필드로 변경

	// 1. Anchor 위치 (좌우 이동)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AnchorPosition = 0.f;

	// 2. HookRope 높이/길이 (위아래 이동)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HookRopeHeight = 0.f;

	// 3. ControllerRope 높이/길이 (위아래 이동)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ControlRopeHeight = 0.f;
};

// 기존 CraneState(1)의 업무 의미를 프로젝트에서 명시적으로 유지한다.
enum class EMa0t10DataType : int32
{
	None = 0,
	CraneState = 1
};

struct FCraneStateData : public FDxDataBase
{
	UPROPERTY(BlueprintReadWrite)
	FString CraneId;

	UPROPERTY(BlueprintReadWrite)
	FCranePositionData Position;

	UPROPERTY(BlueprintReadWrite)
	FString OperationStatus;

	virtual int32 GetType() const override
	{
		return static_cast<int32>(EMa0t10DataType::CraneState);
	}
};
