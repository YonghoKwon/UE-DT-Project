// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ma0t10_dt : ModuleRules
{
    public ma0t10_dt(ReadOnlyTargetRules Target) : base(Target)
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
            "RHI",
            "ImageWrapper",
            "Json",
            "ProceduralMeshComponent",
            "Sockets",
            "Networking",
            "HTTPServer"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "DesktopPlatform",
                "UnrealEd"
            });
        }
    }
}
