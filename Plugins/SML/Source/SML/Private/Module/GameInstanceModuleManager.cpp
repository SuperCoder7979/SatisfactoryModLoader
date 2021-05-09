#include "Module/GameInstanceModuleManager.h"
#include "SatisfactoryModLoader.h"
#include "ModLoading/PluginModuleLoader.h"
#include "Registry/RemoteCallObjectRegistry.h"
#include "Tooltip/ItemTooltipSubsystem.h"

UGameInstanceModuleManager::UGameInstanceModuleManager() {
    this->bIsInitializingCurrently = false;
    this->CurrentSubsystemCollection = NULL;
}

UGameInstanceModule* UGameInstanceModuleManager::FindModule(const FName& ModReference) const {
    UGameInstanceModule* const* GameInstanceModule = RootModuleMap.Find(ModReference);
    if (GameInstanceModule != NULL) {
        return *GameInstanceModule;
    }
    return NULL;
}

void UGameInstanceModuleManager::Initialize(FSubsystemCollectionBase& Collection) {
    this->bIsInitializingCurrently = true;
    this->CurrentSubsystemCollection = &Collection;
    EnsureSMLSubsystemsInitialized();

    // Discover modules by scanning classpath
    const TArray<FDiscoveredModule> DiscoveredModules = FPluginModuleLoader::FindRootModulesOfType(UGameInstanceModule::StaticClass());

    TMap<FString, FString> AlreadyLoadedMods;
    for (const FDiscoveredModule& Module : DiscoveredModules) {

        // Make sure we are not trying to register a single plugin twice
        if (AlreadyLoadedMods.Contains(Module.OwnerPluginName)) {
            const FString OtherModulePath = AlreadyLoadedMods.FindChecked(Module.OwnerPluginName);
            UE_LOG(
                LogSatisfactoryModLoader,
                Fatal,
                TEXT("Failed to initialize Game Instane Module for Plugin %s: "
                    "Multiple root modules have been discovered, %s and %s. Only one root module is allowed"),
                *Module.OwnerPluginName,
                *OtherModulePath,
                *Module.ModuleClass->GetPathName()
            );
            continue;
        }

        // Register module in the game instance module manager now
        const TSubclassOf<UGameInstanceModule> GameInstanceModule = Module.ModuleClass.Get();
        AlreadyLoadedMods.Add(Module.OwnerPluginName, Module.ModuleClass->GetPathName());
        CreateRootModule(*Module.OwnerPluginName, GameInstanceModule);
    }

    UE_LOG(LogSatisfactoryModLoader, Log, TEXT("Discovered %d game instance modules"), AlreadyLoadedMods.Num());

    // Dispatch lifecycle events in a sequence
    DispatchLifecycleEvent(ELifecyclePhase::CONSTRUCTION);
    DispatchLifecycleEvent(ELifecyclePhase::INITIALIZATION);
    DispatchLifecycleEvent(ELifecyclePhase::POST_INITIALIZATION);

    this->bIsInitializingCurrently = false;
    this->CurrentSubsystemCollection = NULL;
}

void UGameInstanceModuleManager::EnsureSMLSubsystemsInitialized() {
    EnsureSubsystemInitialized(URemoteCallObjectRegistry::StaticClass());
    EnsureSubsystemInitialized(UItemTooltipSubsystem::StaticClass());
}

bool UGameInstanceModuleManager::ShouldCreateSubsystem(UObject* Outer) const {
    // We only want to initialize modules for PIE and game worlds, never for editor and other worlds
    UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
    return FPluginModuleLoader::ShouldLoadModulesForWorld(GameInstance->GetWorld());
}

void UGameInstanceModuleManager::EnsureSubsystemInitialized(const TSubclassOf<UGameInstanceSubsystem> SubsystemClass) {
    if (bIsInitializingCurrently) {
        check(CurrentSubsystemCollection->InitializeDependency(SubsystemClass));
    }
}

void UGameInstanceModuleManager::CreateRootModule(const FName& ModReference, TSubclassOf<UGameInstanceModule> ObjectClass) {
    // Allocate module object and set mod reference
    UGameInstanceModule* RootGameInstanceModule = NewObject<UGameInstanceModule>(this, ObjectClass, ModReference);
    check(RootGameInstanceModule);
    RootGameInstanceModule->SetOwnerModReference(ModReference);

    // Add it to internal lists
    RootModuleMap.Add(ModReference, RootGameInstanceModule);
    RootModuleList.Add(RootGameInstanceModule);
}

void UGameInstanceModuleManager::DispatchLifecycleEvent(ELifecyclePhase Phase) {
    // Notify log of our current loading phase, in case of things going wrong
    UE_LOG(
        LogSatisfactoryModLoader,
        Log,
        TEXT("Dispatching lifecycle event %s to game instance modules"),
        *UModModule::LifecyclePhaseToString(Phase)
    );

    // Iterate modules in their order of registration and dispatch lifecycle event to them
    for (UGameInstanceModule* RootModule : RootModuleList) {
        RootModule->DispatchLifecycleEvent(Phase);
    }
}
