#include "VirtualSensorSemanticLabels.h"
#include "DeviceSemanticPolicy.h"
#include "VirtualLidarScanComponent.h"
#include "Components/PrimitiveComponent.h"

namespace VirtualSensorSemanticLabels
{
const TArray<FVirtualLidarSemanticClassRule>& Defaults()
{
    return DeviceSemanticPolicy::Rules();
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
