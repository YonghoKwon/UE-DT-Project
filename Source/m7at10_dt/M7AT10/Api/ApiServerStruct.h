#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ApiServerStruct.generated.h"

UENUM(BlueprintType)
enum class EApiServer : uint8
{
	Local UMETA(DisplayName = "Local"),
	Private UMETA(DisplayName = "Private"),
	Test UMETA(DisplayName = "Test"),
	Prod UMETA(DisplayName = "Prod")
};

USTRUCT(BlueprintType)
struct FApiServerStruct : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	EApiServer ApiServer;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString ApiServerUrl;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString Login;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString Password;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString Comment;
};
