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
            "MeshDescription",
            "StaticMeshDescription",
            "PhysicsCore",
            "Sockets",
            "Networking",
            "HTTPServer",
            "Niagara",
            "NiagaraCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { "ma0t10_dtBootstrap" });

        // PKG-01: ship the deployment transport settings as a loose file next to every
        // package (<package>/ma0t10_dt/Config/SensorTransport.ini), outside the pak.
        RuntimeDependencies.Add("$(ProjectDir)/Config/SensorTransport.ini", StagedFileType.NonUFS);
        // TESTSET-03: the default demo template is available without Editor-only fixtures.
        RuntimeDependencies.Add("$(ProjectDir)/Config/AgentTestSetScenario.json", StagedFileType.NonUFS);

        // PKG-02: dataset provenance must survive deployment without Git or .git.
        // Resolve at build time; never launch a child process in a packaged export.
        string ProjectCommit = "";
        string GitDirectory = System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../.git"));
        string GitHead = System.IO.Path.Combine(GitDirectory, "HEAD");
        if (System.IO.File.Exists(GitHead))
        {
            ExternalDependencies.Add(GitHead);
            string Head = System.IO.File.ReadAllText(GitHead).Trim();
            string Ref = Head.StartsWith("ref: ") ? System.IO.Path.Combine(GitDirectory, Head.Substring(5)) : "";
            if (System.IO.File.Exists(Ref)) ExternalDependencies.Add(Ref);
            string PackedRefs = System.IO.Path.Combine(GitDirectory, "packed-refs");
            if (System.IO.File.Exists(PackedRefs)) ExternalDependencies.Add(PackedRefs);
        }
        var GitInfo = new System.Diagnostics.ProcessStartInfo("git.exe", "rev-parse --verify HEAD")
        {
            WorkingDirectory = ModuleDirectory,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        try
        {
            using (var Git = System.Diagnostics.Process.Start(GitInfo))
            {
                string Candidate = Git.StandardOutput.ReadToEnd().Trim();
                Git.StandardError.ReadToEnd();
                Git.WaitForExit();
                if (Git.ExitCode == 0 && System.Text.RegularExpressions.Regex.IsMatch(Candidate, "^[0-9a-f]{40}$"))
                    ProjectCommit = Candidate;
            }
        }
        catch (System.Exception) { }
        PrivateDefinitions.Add("MA0T10_PROJECT_COMMIT=\"" + ProjectCommit + "\"");

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
