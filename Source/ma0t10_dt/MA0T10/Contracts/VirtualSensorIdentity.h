#pragma once

#include "CoreMinimal.h"

/** Exact legacy role alias. Device profiles and unrelated user IDs are preserved. */
namespace VirtualSensorIdentity
{
inline constexpr TCHAR CoilQuay[] = TEXT("os_lidar_coil_quay");
inline constexpr TCHAR LegacyCoilQuay[] = TEXT("Coil.Quay.XT32");
inline FString Canonical(const FString& Id)
{
    return Id.Equals(LegacyCoilQuay, ESearchCase::CaseSensitive) ? FString(CoilQuay) : Id;
}
template<typename ValueType>
inline void MigrateCoilQuayKey(TMap<FString,ValueType>& Values)
{
    ValueType Legacy;
    if(Values.RemoveAndCopyValue(LegacyCoilQuay,Legacy)&&!Values.Contains(CoilQuay))Values.Add(CoilQuay,MoveTemp(Legacy));
}
}
