#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Device/Contracts/DeviceIdentityPolicy.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"
#include "Device/Sensor/DeviceSemanticPolicy.h"
#include "Device/Sensor/VirtualSensorSemanticLabels.h"
#include "Device/Sensor/VirtualLidarScanComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceHostPolicyCompatibility,"MA0T10.DeviceBoundary.HostPolicyCompatibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceHostPolicyCompatibility::RunTest(const FString&)
{
    const FName Labels[]={TEXT("Slab"),TEXT("Roller"),TEXT("Conveyor"),TEXT("Floor"),TEXT("Sensor"),TEXT("Coil")};
    TestEqual(TEXT("host rule count"),VirtualSensorSemanticLabels::Defaults().Num(),6);
    for(int32 I=0;I<6;++I)
    {
        TestEqual(TEXT("wire semantic ID unchanged"),VirtualSensorSemanticLabels::ClassId(Labels[I]),uint8(I+2));
        TestEqual(TEXT("semantic label unchanged"),VirtualSensorSemanticLabels::Label(uint8(I+2)),Labels[I]);
    }
    TestEqual(TEXT("unknown label ID"),VirtualSensorSemanticLabels::ClassId(TEXT("unregistered-device-class")),uint8(1));
    TestEqual(TEXT("background ID"),VirtualSensorSemanticLabels::Label(0),FName(TEXT("Background")));
    TestEqual(TEXT("legacy alias"),DeviceIdentityPolicy::Canonical(TEXT("Coil.Quay.XT32")),FString(TEXT("os_lidar_coil_quay")));
    TestEqual(TEXT("case-sensitive alias"),DeviceIdentityPolicy::Canonical(TEXT("coil.Quay.XT32")),FString(TEXT("coil.Quay.XT32")));
    TMap<FString,int32> Saved{{TEXT("Coil.Quay.XT32"),42},{TEXT("os_lidar_coil_quay"),17}};
    VirtualSensorIdentity::MigrateCoilQuayKey(Saved);
    TestFalse(TEXT("old saved key removed"),Saved.Contains(TEXT("Coil.Quay.XT32")));
    TestEqual(TEXT("existing canonical data wins"),Saved.FindChecked(TEXT("os_lidar_coil_quay")),17);
    const auto OriginalAliases=DeviceIdentityPolicy::Aliases();
    ON_SCOPE_EXIT {DeviceIdentityPolicy::Configure(OriginalAliases);};
    DeviceIdentityPolicy::Configure({{TEXT("old-custom"),TEXT("new-custom")}});
    TestEqual(TEXT("runtime supports host-independent aliases"),DeviceIdentityPolicy::Canonical(TEXT("old-custom")),FString(TEXT("new-custom")));
    TestEqual(TEXT("legacy facade forwards host policy"),VirtualSensorIdentity::Canonical(TEXT("old-custom")),FString(TEXT("new-custom")));
    return true;
}
#endif
