// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;
using System;
using System.Runtime.InteropServices;
using System.Text;
using Tools.DotNETCommon;

public class SML : ModuleRules {
    public SML(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        // SML transitive dependencies
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Json",
            "MovieScene",
            "Projects"
        });
        PrivateDependencyModuleNames.AddRange(new[] {"RenderCore"});

        PublicDependencyModuleNames.AddRange(new string[] {"FactoryGame"});

        // FactoryGame transitive dependencies
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject",
            "Engine",
            "InputCore",
            "OnlineSubsystem", "OnlineSubsystemNull", "OnlineSubsystemEOS", "OnlineSubsystemUtils",
            "SignificanceManager",
            "APEX", "ApexDestruction",
            "AnimGraphRuntime",
            "AkAudio",
            "PhysXVehicles",
            "AssetRegistry",
            "NavigationSystem",
            "ReplicationGraph",
            "AIModule",
            "GameplayTasks",
            "SlateCore", "Slate", "UMG",
            "InstancedSplines"
        });

        if (Target.bBuildEditor) {
            PublicDependencyModuleNames.Add("UnrealEd");
            PrivateDependencyModuleNames.Add("MainFrame");
        }

        var thirdPartyFolder = Path.Combine(ModuleDirectory, "../../ThirdParty");
        PublicIncludePaths.Add(Path.Combine(thirdPartyFolder, "include"));

        var platformName = Target.Platform.ToString();
        var libraryFolder = Path.Combine(thirdPartyFolder, platformName);

        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "funchook.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "detex.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "libfbxsdk-md.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "libxml2-md.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "zlib-md.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "AssemblyAnalyzer.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "Zydis.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libraryFolder, "Zycore.lib"));

        // Collect build metadata from the environment and pass it to C++
        var currentBranch = Environment.GetEnvironmentVariable("BRANCH_NAME");
        var buildId = Environment.GetEnvironmentVariable("BUILD_NUMBER");
        if (currentBranch == null) {
            RetrieveHeadBranchAndCommitFromGit(Target.ProjectFile.Directory, out currentBranch, out buildId);
            if (buildId != null && buildId.Length > 8) buildId = buildId.Substring(0, 8);
        }

        Log.TraceInformation("Environment: Branch = {0} BuildId = {1}", currentBranch, buildId);
        if (currentBranch != null && buildId != null) {
            var buildMetadataString = currentBranch == "master" ? buildId : string.Format("{0}.{1}", currentBranch, buildId);
            PrivateDefinitions.Add(string.Format("SML_BUILD_METADATA=\"{0}\"", buildMetadataString));
        }
    }

    private static void RetrieveHeadBranchAndCommitFromGit(DirectoryReference RootDir, out string branchName, out string commitRef) {
        branchName = null;
        commitRef = null;

        var gitRepository = Path.Combine(RootDir.FullName, ".git");
        if (!Directory.Exists(gitRepository)) return;
        var gitHeadFile = Path.Combine(gitRepository, "HEAD");
        if (!File.Exists(gitHeadFile)) return;

        try {
            var headFileContents = File.ReadAllText(gitHeadFile).Replace("\n", "");
            ;

            // It is a normal HEAD ref, so it's name should equal to local branch name
            if (headFileContents.StartsWith("ref: refs/heads/")) {
                branchName = headFileContents.Substring(16);
                // Try to resolve commit name directly from the ref file
                var headRefFile = Path.Combine(gitRepository, "refs", "heads", branchName);
                if (!File.Exists(headRefFile)) return;

                var headRefFileContents = File.ReadAllText(headRefFile).Replace("\n", "");
                commitRef = headRefFileContents;
                return;
            }

            Log.TraceWarning("Git HEAD does not refer to a branch, are we in a detached head state? HEAD: {0}", headFileContents);
            branchName = "detached-head";
            commitRef = headFileContents;
        }
        catch (Exception ex) {
            Log.TraceWarning("Failed to handle git HEAD file at {0}: {1}", gitHeadFile, ex.Message);
        }
    }
}