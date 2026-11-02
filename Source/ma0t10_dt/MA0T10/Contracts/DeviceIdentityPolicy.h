#pragma once
#include "CoreMinimal.h"
struct FDeviceIdentityAlias { FString Legacy,Canonical; };
/** Host-owned aliases, installed before any layout/recording is loaded. */
namespace DeviceIdentityPolicy
{
    MA0T10_DT_API void Configure(TArray<FDeviceIdentityAlias> Aliases);
    MA0T10_DT_API const TArray<FDeviceIdentityAlias>& Aliases();
    MA0T10_DT_API FString Canonical(const FString& Id);
    MA0T10_DT_API bool ContainsLegacy(const FString& Text);
    template<typename ValueType> void MigrateAliases(TMap<FString,ValueType>& Values)
    {
        for(const auto& Alias:Aliases())
        {
            ValueType Value;
            if(Values.RemoveAndCopyValue(Alias.Legacy,Value)&&!Values.Contains(Alias.Canonical))
                Values.Add(Alias.Canonical,MoveTemp(Value));
        }
    }
}
