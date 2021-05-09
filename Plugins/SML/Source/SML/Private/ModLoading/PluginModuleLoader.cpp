#include "ModLoading/PluginModuleLoader.h"
#include "AssetRegistryModule.h"
#include "FGGameMode.h"
#include "IPlatformFilePak.h"
#include "Util/BlueprintAssetHelperLibrary.h"
#include "SatisfactoryModLoader.h"

// Switch to enable mod loading in editor. Currently it's disabled because we don't have proper FactoryGame editor build
#ifndef ENABLE_MOD_LOADING_IN_EDITOR
#define ENABLE_MOD_LOADING_IN_EDITOR 0
#endif

static bool PluginModuleLoaderExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) {
    if (FParse::Command(&Cmd, TEXT("TestPluginModuleLoader"))) {
        TArray<FDiscoveredModule> DiscoveredModules = FPluginModuleLoader::FindRootModulesOfType(UModModule::StaticClass());

        Ar.Logf(TEXT("Found %d plugin root modules: "), DiscoveredModules.Num());

        for (const FDiscoveredModule& DiscoveredModule : DiscoveredModules) {
            UClass* NativeParentClass = GetParentNativeClass(DiscoveredModule.ModuleClass);
            Ar.Logf(TEXT("[%s] %s - Superclass: %s"), *DiscoveredModule.OwnerPluginName, *DiscoveredModule.ModuleClass->GetPathName(), *NativeParentClass->GetPathName());
        }
        return true;
    }
    if (FParse::Command(&Cmd, TEXT("TestContentOwnerResolution"))) {
        const FString TargetPackageName = Cmd;
        const FString OwnerPluginName = UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(TargetPackageName, false);
        Ar.Logf(TEXT("Owner of the package %s is %s"), *TargetPackageName, *OwnerPluginName);
        return true;
    }
    return false;
}

TArray<FDiscoveredModule> FPluginModuleLoader::FindRootModulesOfType(TSubclassOf<UModModule> ModuleType) {
    TArray<FDiscoveredModule> ResultingModules;

    // Retrieve all loaded classes parenting from module class and check them
    TArray<UClass*> NativeModuleClasses;
    UBlueprintAssetHelperLibrary::FindNativeClassesByType(ModuleType, NativeModuleClasses);

    // Retrieve assets with bRootModule tag set to true using asset registry
    TArray<UClass*> BlueprintModuleClasses;
    UBlueprintAssetHelperLibrary::FindBlueprintAssetsByTag(ModuleType, TEXT("bRootModule"), {TEXT("True")}, BlueprintModuleClasses);

    TSet<UClass*> AllModuleClasses;
    AllModuleClasses.Reserve(NativeModuleClasses.Num() + BlueprintModuleClasses.Num());
    AllModuleClasses.Append(NativeModuleClasses);
    AllModuleClasses.Append(BlueprintModuleClasses);

    // Iterate found classes and perform additional checking
    for (UClass* RootModuleClass : AllModuleClasses) {
        if (!RootModuleClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated)) {
            UModModule* ModModuleCDO = CastChecked<UModModule>(RootModuleClass->GetDefaultObject());

            // Make sure it is a root module after all, because FindNativeClassesByType doesn't check for this
            if (ModModuleCDO->bRootModule) {
                // Retrieve owning plugin name for the provided class
                const FString OwnerPluginName = UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(RootModuleClass->GetPathName());

                // Make sure valid owner has been found
                if (OwnerPluginName.IsEmpty()) {
                    UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Failed to determine owning plugin for root module %s"), *RootModuleClass->GetPathName());
                    continue;
                }

                // Add module into the collection
                ResultingModules.Add(FDiscoveredModule{OwnerPluginName, RootModuleClass});
            }
        }
    }
    return ResultingModules;
}

bool FPluginModuleLoader::ShouldLoadModulesForWorld(UWorld* World) {
    // Disable module loading in editor if it's disabled by the macro
#if WITH_EDITOR && ENABLE_MOD_LOADING_IN_EDITOR == 0
    return false;
#else
    return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
#endif
}

bool FPluginModuleLoader::IsMainMenuWorld(UWorld* World) {
    AFGGameMode* GameMode = Cast<AFGGameMode>(World->GetAuthGameMode());
    return GameMode != NULL && GameMode->IsMainMenuGameMode();
}

static FStaticSelfRegisteringExec PluginModuleLoaderExecRegistration(&PluginModuleLoaderExec);
