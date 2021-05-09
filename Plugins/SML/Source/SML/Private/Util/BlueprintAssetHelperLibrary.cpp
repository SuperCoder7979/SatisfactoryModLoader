#include "Util/BlueprintAssetHelperLibrary.h"
#include "AssetRegistryModule.h"
#include "SatisfactoryModLoader.h"
#include "Interfaces/IPluginManager.h"
#include "ModLoading/ModLoadingLibrary.h"

void UBlueprintAssetHelperLibrary::FindBlueprintAssetsByTag(UClass* BaseClass, const FName TagName, const TArray<FString>& TagValues, TArray<UClass*>& FoundAssets) {

    // Collect asset tag values into the multi map
    TMultiMap<FName, FString> RequiredTagsMap;
    for (const FString& Value : TagValues) {
        RequiredTagsMap.Add(TagName, Value);
    }

    // Query content registry for assets
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> OutFoundAssets;
    AssetRegistry.GetAssetsByTagValues(RequiredTagsMap, OutFoundAssets);

    for (const FAssetData& AssetData : OutFoundAssets) {
        // Make sure found asset is a blueprint
        if (AssetData.AssetClass != TEXT("Blueprint")) {
            continue;
        }

        // Retrieve GeneratedClass tag containing a text path to generated class
        FString GeneratedClassExportedPath;
        if (!AssetData.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassExportedPath)) {
            continue;
        }

        // Make sure export path represents a valid path and convert it to pure objectt path
        FString GeneratedClassPath;
        if (!FPackageName::ParseExportTextPath(GeneratedClassExportedPath, NULL, &GeneratedClassPath)) {
            continue;
        }

        // Load UBlueprintGeneratedClass for provided object and make sure it has been loaded
        UClass* ClassObject = LoadObject<UClass>(NULL, *GeneratedClassPath);
        if (ClassObject == NULL) {
            continue;
        }

        // Verify that generated class is actually a child of the base class, and then add it to the list
        if (ClassObject->IsChildOf(BaseClass)) {
            FoundAssets.Add(ClassObject);
        }
    }
}

void UBlueprintAssetHelperLibrary::FindNativeClassesByType(UClass* BaseClass, TArray<UClass*>& FoundClasses) {
    GetDerivedClasses(BaseClass, FoundClasses, true);
}

FString FindOwnerPluginForModuleName(const FString& ModuleName, bool bTreatNonModPluginsAsGame) {
    // Iterate mod plugin modules and find the owning plugin for them
    const TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins) {
        if (UModLoadingLibrary::IsPluginAMod(Plugin.Get()) || !bTreatNonModPluginsAsGame) {
            for (const FModuleDescriptor& ModuleDescriptor : Plugin->GetDescriptor().Modules) {
                if (ModuleDescriptor.Name == *ModuleName) {
                    return Plugin->GetName();
                }
            }
        }
    }

    // If package is not owned by any of the mod modules, we assume it's game or engine native module
    // In that case, it belongs to satisfactory itself
    return FACTORYGAME_MOD_NAME;
}

FString FindOwnerPluginForMountPoint(const FString& MountPoint, bool bTreatNonModPluginsAsGame) {
    const TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPluginsWithContent();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins) {

        // Check whenever given mount point is owned by this plugin
        const FString PluginMountPath = Plugin->GetMountedAssetPath();
        if (PluginMountPath.Mid(1, PluginMountPath.Len() - 2) == MountPoint) {
            // We only want to use plugin name for mods
            if (UModLoadingLibrary::IsPluginAMod(Plugin.Get()) || !bTreatNonModPluginsAsGame) {
                return Plugin->GetName();
            }

            // This mount point is plugin owned, but does not represent a mod
            // Assume FactoryGame/Engine plugin
            return FACTORYGAME_MOD_NAME;
        }
    }

    // Return empty string if we haven't found any associated plugin
    return TEXT("");
}

FString UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(const FString& ObjectPath, bool bTreatNonModPluginsAsGame) {
    // Retrieve mount point for package name
    const FString PackageMountPoint = FPackageName::GetPackageMountPoint(ObjectPath).ToString();

    // Make sure that package name represents a valid mount point
    if (PackageMountPoint == TEXT("None")) {
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("FindPluginNameByObjectPath: received invalid path with no associated mount point: %s"), *ObjectPath);
        return TEXT("");
    }

    // Game mount point is owned by Satisfactory itself, since we now have a strong content separation between mods and the game
    // We use StartsWith here because editor can create sub-mounts under /Game/ for representing shared content packs
    if (PackageMountPoint.StartsWith(TEXT("Game"))) {
        return FACTORYGAME_MOD_NAME;
    }
    // Engine content is owned by the game, although we might want to make a separation later
    if (PackageMountPoint == TEXT("Engine")) {
        return FACTORYGAME_MOD_NAME;
    }

    // Script mount point is used for accessing native class/CDO objects, package name will equal Module Name,
    // and we can figure out module owner by iterating plugin manifests and assuming FactoryGame if it's not found
    if (PackageMountPoint == TEXT("Script")) {
        FString PackageName;
        int32 FirstDotIndex;
        if (ObjectPath.FindChar(TEXT('.'), FirstDotIndex)) {
            PackageName = ObjectPath.Mid(8, FirstDotIndex - 8);
        } else {
            PackageName = ObjectPath.Mid(8);
        }
        if (!FModuleManager::Get().IsModuleLoaded(*PackageName)) {
            UE_LOG(LogSatisfactoryModLoader, Error, TEXT("FindPluginNameByObjectPath: Found /Script/ package not representing a loaded module: %s"), *PackageName);
            return TEXT("");
        }
        return FindOwnerPluginForModuleName(PackageName, bTreatNonModPluginsAsGame);
    }

    // Assume all configs to be owned by the FactoryGame, and for Memory/Temp we cannot really make a distinction,
    // so we assume they are Engine-owned, and belong to FactoryGame too
    if (PackageMountPoint == TEXT("Config") ||
        PackageMountPoint == TEXT("Memory") ||
        PackageMountPoint == TEXT("Temp")) {
        return FACTORYGAME_MOD_NAME;
    }

    // Only remaining case are additional mount points representing plugins and various subsystems
    // We cannot really handle all of them properly, so we handle only plugins for now
    const FString ResultName = FindOwnerPluginForMountPoint(PackageMountPoint, bTreatNonModPluginsAsGame);

    // Make sure path represents a valid plugin
    if (ResultName.IsEmpty()) {
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("FindPluginNameByObjectPath: Encountered unexpected mount point: %s"), *PackageMountPoint);
        return TEXT("");
    }

    return ResultName;
}
