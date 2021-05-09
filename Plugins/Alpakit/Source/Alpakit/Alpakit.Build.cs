// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Alpakit : ModuleRules {
    public Alpakit(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Projects",
            "InputCore",
            "UnrealEd",
            "LevelEditor",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore"
        });
    }
}