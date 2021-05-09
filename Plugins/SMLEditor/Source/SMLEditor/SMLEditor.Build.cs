using UnrealBuildTool;
using System.IO;
using System;

public class SMLEditor : ModuleRules {
    public SMLEditor(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {"SML", "FactoryGame", "UnrealEd", "BlueprintGraph"});
        bLegacyPublicIncludePaths = false;

        // SML transitive dependencies
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Json"
        });

        // FactoryGame transitive dependencies
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject",
            "Engine",
            "InputCore",
            "OnlineSubsystem", "OnlineSubsystemNull", "OnlineSubsystemEOS", "OnlineSubsystemUtils",
            "SignificanceManager",
            "APEX", "ApexDestruction",
            "BlueprintGraph",
            "KismetCompiler",
            "AnimGraphRuntime",
            "AkAudio",
            "PhysXVehicles",
            "AssetRegistry",
            "NavigationSystem",
            "ReplicationGraph",
            "AIModule",
            "GameplayTasks",
            "SlateCore", "Slate", "UMG",
            "InstancedSplines",
            "Projects"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "DesktopPlatform"
        });
    }
}