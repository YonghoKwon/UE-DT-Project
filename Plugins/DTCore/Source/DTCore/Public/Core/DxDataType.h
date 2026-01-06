#pragma once

#include "CoreMinimal.h"
#include "DxDataType.generated.h"

/**
 * @brief
 */
UENUM()
enum class EDxDataType : uint8
{
	None UMETA(DisplayName = "None"), // 초기화되지 않은 상태를 위해 기본값(0) 세팅

	// 추가가 필요한 구조체들...
	CraneState,
	ShipState,

	//...
};

// 모든 데이터 구조체의 부모가 될 구조체
struct DTCORE_API FDxDataBase
{
	virtual ~FDxDataBase() = default; // 가상 소멸자 필수 (메모리 누수 방지)

	// 자신이 무슨 타입인지 알려주는 순수 가상 함수
	virtual EDxDataType GetType() const = 0;
};