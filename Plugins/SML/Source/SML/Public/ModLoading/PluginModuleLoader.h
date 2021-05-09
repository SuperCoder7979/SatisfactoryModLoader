#pragma once
#include "CoreMinimal.h"
#include "Module/ModModule.h"

/** Describes a single discovered mod root module associated with it's owner plugin name */
struct SML_API FDiscoveredModule {
    FString OwnerPluginName;
    TSubclassOf<UModModule> ModuleClass;
};

class SML_API FPluginModuleLoader {
public:
    /** Retrieves all root modules of the provided type and their respective owners */
    static TArray<FDiscoveredModule> FindRootModulesOfType(TSubclassOf<UModModule> ModuleType);

    /** Determines whenever we want to load modules for the provided world. Generally, we want to load modules only for standalone and PIE worlds */
    static bool ShouldLoadModulesForWorld(UWorld* World);

    /** Returns true if this world represents a main menu world */
    static bool IsMainMenuWorld(UWorld* World);
};
