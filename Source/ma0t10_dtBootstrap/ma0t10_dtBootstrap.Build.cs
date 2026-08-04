using UnrealBuildTool;

public class ma0t10_dtBootstrap : ModuleRules
{
    public ma0t10_dtBootstrap(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("Core");
    }
}
