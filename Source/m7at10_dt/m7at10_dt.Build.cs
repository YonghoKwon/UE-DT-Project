// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class m7at10_dt : ModuleRules
{
    public m7at10_dt(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "WebSockets",
            "HTTP",
            "Stomp",
            "EnhancedInput",
            "UMG",
            "Slate",
            "SlateCore",
            "PixelStreaming",
            "PixelStreamingInput",
            "PixelStreamingBlueprint",
            "DTCore",
            "RenderCore",
            "ImageWrapper",
            "Json"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
