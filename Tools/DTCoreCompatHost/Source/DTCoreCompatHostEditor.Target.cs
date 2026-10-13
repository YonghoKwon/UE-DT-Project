using UnrealBuildTool;
public class DTCoreCompatHostEditorTarget : TargetRules
{
    public DTCoreCompatHostEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("DTCoreCompatHost");
    }
}
