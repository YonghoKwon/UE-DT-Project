#pragma once
#include "CoreMinimal.h"
struct FVirtualLidarSemanticClassRule;
/** Configure once at module startup, before acquisition; do not mutate while readers are active. */
namespace DeviceSemanticPolicy
{
    MA0T10_DT_API void Configure(TArray<FVirtualLidarSemanticClassRule> Rules,TMap<FName,float> Reflectances,FName DiagnosticLabel,FName DiagnosticTag);
    MA0T10_DT_API const TArray<FVirtualLidarSemanticClassRule>& Rules();
    MA0T10_DT_API float Reflectance(FName Label);
    MA0T10_DT_API FName DiagnosticLabel();
    MA0T10_DT_API FName DiagnosticTag();
}
