#include "DeviceIdentityPolicy.h"
namespace { TArray<FDeviceIdentityAlias> HostAliases; }
void DeviceIdentityPolicy::Configure(TArray<FDeviceIdentityAlias> Aliases)
{ check(IsInGameThread()); HostAliases=MoveTemp(Aliases); }
const TArray<FDeviceIdentityAlias>& DeviceIdentityPolicy::Aliases() { return HostAliases; }
FString DeviceIdentityPolicy::Canonical(const FString& Id)
{
    for(const auto& Alias:HostAliases)if(Id.Equals(Alias.Legacy,ESearchCase::CaseSensitive))return Alias.Canonical;
    return Id;
}
bool DeviceIdentityPolicy::ContainsLegacy(const FString& Text)
{
    for(const auto& Alias:HostAliases)if(Text.Contains(Alias.Legacy,ESearchCase::CaseSensitive))return true;
    return false;
}
