#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DxLevelStruct.generated.h"

class UDxWidget;
enum class EDxViewMode : uint8;

USTRUCT(BlueprintType)
struct FDxLevelStruct : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	TSoftObjectPtr<UWorld> Level;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FString LevelName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FString LevelComment;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	bool IsUse;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	bool IsDefault;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	EDxViewMode DxViewMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	TSubclassOf<UDxWidget> UseMainWidget;
};
