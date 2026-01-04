#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WebSocketServerStruct.generated.h"

UENUM(BlueprintType)
enum class EWebSocketServer : uint8
{
	Local UMETA(DisplayName = "Local"),
	Private UMETA(DisplayName = "Private"),
	Test UMETA(DisplayName = "Test"),
	Prod UMETA(DisplayName = "Prod")
};

USTRUCT(BlueprintType)
struct FWebSocketServerStruct : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	EWebSocketServer WebSocketServer;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString WebSocketServerUrl;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString Login;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString Password;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebSocketServer")
	FString Comment;
};
