#include "ProjectDeviceSemanticPolicy.h"
#include "Device/Sensor/DeviceSemanticPolicy.h"
#include "Device/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"
namespace
{
const TArray<FVirtualLidarSemanticClassRule>& ProjectRules()
{
    static const TArray<FVirtualLidarSemanticClassRule> Value=[]
    {
    TArray<FVirtualLidarSemanticClassRule> Rules;
    auto AddRule = [&Rules](FName Label, const FLinearColor& Color, TArray<FName> Tags, TArray<FName> Classes, TArray<FString> Names, TArray<FString> Comps)
    {
        FVirtualLidarSemanticClassRule R; R.Label = Label; R.DisplayColor = Color; R.ActorTags = Tags; R.ActorClassNames = Classes; R.ActorNameContains = Names; R.ComponentNameContains = Comps; Rules.Add(R);
    };
    AddRule(TEXT("Slab"), FLinearColor(1.0f, 0.25f, 0.02f, 1.0f), {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("SteelSlab"), TEXT("Plate")}, {}, {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("SteelSlab"), TEXT("Plate")}, {TEXT("Slab"), TEXT("slab"), TEXT("Plate")});
    AddRule(TEXT("Roller"), FLinearColor(0.0f, 0.85f, 1.0f, 1.0f), {TEXT("Roller"), TEXT("roller"), TEXT("Rollers")}, {}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll"), TEXT("roll")}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll")});
    AddRule(TEXT("Conveyor"), FLinearColor(0.05f, 1.0f, 0.25f, 1.0f), {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Equipment")}, {}, {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Frame"), TEXT("Equipment")}, {TEXT("Conveyor"), TEXT("Frame"), TEXT("Equipment")});
    AddRule(TEXT("Floor"), FLinearColor(0.2f, 0.25f, 1.0f, 1.0f), {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground")}, {}, {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground"), TEXT("Grid")}, {TEXT("Floor"), TEXT("Ground")});
    AddRule(TEXT("Sensor"), FLinearColor(0.75f, 0.15f, 1.0f, 1.0f), {TEXT("Sensor"), TEXT("Lidar"), TEXT("LiDAR"), TEXT("Camera")}, {}, {TEXT("Sensor"), TEXT("Lidar"), TEXT("LiDAR"), TEXT("Camera")}, {TEXT("Sensor"), TEXT("Lidar"), TEXT("Camera")});
    // Append only: existing semantic IDs2..6 and all existing wire layouts stay stable.
    AddRule(TEXT("Coil"), FLinearColor(.95f,.65f,.15f), {TEXT("Coil")}, {}, {}, {});
        return Rules;
    }();
    return Value;
}
}
void RegisterProjectDeviceSemanticPolicy()
{
    DeviceSemanticPolicy::Configure(ProjectRules(),{{TEXT("Slab"),.20f},{TEXT("Roller"),.35f},{TEXT("Floor"),.25f},{TEXT("Conveyor"),.12f}},TEXT("Slab"),TEXT("Slab"));
    DeviceIdentityPolicy::Configure({{VirtualSensorIdentity::LegacyCoilQuay,VirtualSensorIdentity::CoilQuay}});
}
