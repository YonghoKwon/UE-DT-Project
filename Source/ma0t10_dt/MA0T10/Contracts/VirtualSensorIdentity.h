#pragma once

#include "CoreMinimal.h"
#include "DeviceIdentityPolicy.h"

/** Exact legacy role alias. Device profiles and unrelated user IDs are preserved. */
namespace VirtualSensorIdentity
{
inline constexpr TCHAR CoilQuay[] = TEXT("os_lidar_coil_quay");
inline constexpr TCHAR LegacyCoilQuay[] = TEXT("Coil.Quay.XT32");
inline FString Canonical(const FString& Id) { return DeviceIdentityPolicy::Canonical(Id); }
template<typename ValueType> inline void MigrateCoilQuayKey(TMap<FString,ValueType>& Values)
{ DeviceIdentityPolicy::MigrateAliases(Values); }
}
