using UnrealBuildTool;

public class ma0t10_dtBootstrap : ModuleRules
{
    public ma0t10_dtBootstrap(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.AddRange(new string[] { "Engine", "RenderCore", "RHI", "Renderer" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D12RHI");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
        }
    }
}
