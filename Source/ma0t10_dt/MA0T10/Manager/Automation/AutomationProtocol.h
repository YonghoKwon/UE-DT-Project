#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace MA0T10Automation
{
constexpr int32 MaxDocumentBytes=256*1024;
struct FJob
{
    FString ResultPath;
    double TimeoutSeconds=3600;
    TArray<TSharedPtr<FJsonObject>> Steps;
};
MA0T10_DT_API FString Json(const TSharedRef<FJsonObject>& Object);
MA0T10_DT_API bool ParseObject(const FString& Text,TSharedPtr<FJsonObject>& Object);
MA0T10_DT_API bool ValidateCommand(const FJsonObject& Command,FString& Error);
MA0T10_DT_API bool ParseJob(const FString& Text,const FString& BaseDirectory,FJob& Job,FString& Error);
MA0T10_DT_API bool ReadDocument(const FString& Path,FString& Text,int64 Limit=MaxDocumentBytes);
}
