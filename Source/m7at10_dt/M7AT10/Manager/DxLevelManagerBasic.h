#pragma once

#include "CoreMinimal.h"
#include "Manager/DxLevelManagerBase.h"
#include "DxLevelManagerBasic.generated.h"

UCLASS()
class M7AT10_DT_API ADxLevelManagerBasic : public ADxLevelManagerBase
{
	GENERATED_BODY()

public:
	ADxLevelManagerBasic();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};
