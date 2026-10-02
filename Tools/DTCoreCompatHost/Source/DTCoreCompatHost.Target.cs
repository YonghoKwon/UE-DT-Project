using UnrealBuildTool;
public class DTCoreCompatHostTarget : TargetRules
{
    public DTCoreCompatHostTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("DTCoreCompatHost");
    }
}
