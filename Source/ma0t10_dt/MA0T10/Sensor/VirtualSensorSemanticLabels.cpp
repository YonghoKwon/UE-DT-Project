#include "VirtualSensorSemanticLabels.h"
#include "VirtualLidarScanComponent.h"
#include "Components/PrimitiveComponent.h"

namespace VirtualSensorSemanticLabels
{
const TArray<FVirtualLidarSemanticClassRule>& Defaults()
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
bool Matches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component)
{
    if (!Actor) return false;
    for (const FName& Tag : Rule.ActorTags) { if (!Tag.IsNone() && Actor->ActorHasTag(Tag)) return true; }
    const FString ActorName = Actor->GetName();
    for (const FString& Pattern : Rule.ActorNameContains) { if (!Pattern.IsEmpty() && ActorName.Contains(Pattern, ESearchCase::IgnoreCase)) return true; }
    const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
    for (const FName& ClassRule : Rule.ActorClassNames) { const FString Pattern = ClassRule.ToString(); if (!Pattern.IsEmpty() && (ClassName.Equals(Pattern, ESearchCase::IgnoreCase) || ClassName.Contains(Pattern, ESearchCase::IgnoreCase))) return true; }
    if (Component) { const FString CompName = Component->GetName(); for (const FString& Pattern : Rule.ComponentNameContains) { if (!Pattern.IsEmpty() && CompName.Contains(Pattern, ESearchCase::IgnoreCase)) return true; } }
    return false;
}
FName Resolve(const UPrimitiveComponent* Component)
{
    const AActor* Actor=Component?Component->GetOwner():nullptr;
    for(const auto& Rule:Defaults()) if(Matches(Rule,Actor,Component)) return Rule.Label;
    return TEXT("Other");
}
uint8 ClassId(FName Name)
{
    for(int32 I=0;I<Defaults().Num();++I) if(Defaults()[I].Label==Name) return uint8(I+2);
    return 1;
}
FName Label(uint8 Id) { return Id==0?FName(TEXT("Background")):Id>=2&&Id<Defaults().Num()+2?Defaults()[Id-2].Label:FName(TEXT("Other")); }
FColor Color(uint8 Id) { return Id==0?FColor::Black:Id>=2&&Id<Defaults().Num()+2?Defaults()[Id-2].DisplayColor.ToFColor(true):FLinearColor(.55f,.55f,.55f,1.f).ToFColor(true); }
}
