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
            "ApplicationCore",
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
			"Json", "JsonUtilities",
            "ProceduralMeshComponent",
            "Sockets",
            "Networking",
            "HTTPServer",
            "Niagara",
            "NiagaraCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // PKG-01: ship the deployment transport settings as a loose file next to every
        // package (<package>/ma0t10_dt/Config/SensorTransport.ini), outside the pak.
        RuntimeDependencies.Add("$(ProjectDir)/Config/SensorTransport.ini", StagedFileType.NonUFS);

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
