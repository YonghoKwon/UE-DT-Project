using UnrealBuildTool;

public class m7at10_dtEditor : ModuleRules
{
    public m7at10_dtEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "DTCore",
            "m7at10_dt"
        });
    }
}
