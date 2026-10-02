using UnrealBuildTool;

public class ma0t10_dtEditor : ModuleRules
{
    public ma0t10_dtEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("UMG");
        PrivateDependencyModuleNames.Add("HTTP");
        PrivateDependencyModuleNames.Add("InputCore");
        PrivateDependencyModuleNames.Add("Json");
        PrivateDependencyModuleNames.Add("Stomp");
        PrivateDependencyModuleNames.AddRange(new[] { "BlueprintGraph", "UMGEditor", "KismetCompiler" });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "Slate",
            "SlateCore",
            "DTCore",
            "ma0t10_dt",
            "AssetTools",
            "MaterialEditor",
            "Niagara",
            "NiagaraCore",
            "NiagaraEditor"
        });
    }
}
