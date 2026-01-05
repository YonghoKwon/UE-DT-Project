#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DxGameStateBase.generated.h"

// 뷰 모드 정의
UENUM(BlueprintType)
enum class EDxViewMode : uint8
{
	None,
	Basic,
	Test,

	Monitoring,   // 3D 모니터링 모드
	Simulation,   // 시뮬레이션 모드
	Replay,       // 리플레이 모드
	Plan,         // 도면 모드
	WorkShop      // 워크샵 모드, 테스트 레벨
};

/**
 *
 */
UCLASS()
class DTCORE_API ADxGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

	// Function
public:
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "ViewMode")
	EDxViewMode CurrentViewMode;
private:
protected:
};
