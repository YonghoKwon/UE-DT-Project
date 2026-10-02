using UnrealBuildTool;
public class DTCoreCompatHost : ModuleRules
{
    public DTCoreCompatHost(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DTCore" });
    }
}
