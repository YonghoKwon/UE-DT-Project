using UnrealBuildTool;

public class DTCore : ModuleRules
{
	public DTCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UMG"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"EnhancedInput",
				"HTTP",
				"WebSockets",
				"Stomp",
				"Slate",
				"SlateCore",
				"DeveloperSettings"
			}
		);
	}
}