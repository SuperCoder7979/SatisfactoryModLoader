#pragma once
#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Util/SemVersion.h"
#include "ModLoadingLibrary.generated.h"

class UTexture2D;
class IPlugin;
class FJsonObject;

/** Mod name of the Satisfactory itself, backend by the dummy mod info with changelist-defined version number */
#define FACTORYGAME_MOD_NAME TEXT("FactoryGame")

/**
 * Represents a simplified info about the mod plugin
 * Data is mostly gathered from the plugin registry and parsing additional metadata
 * declared in the plugin file for SML
 */
USTRUCT(BlueprintType)
struct SML_API FModInfo {
    GENERATED_BODY()
    /**
     * Codename of the mod, used for identifying it in the system and among other mods
     * Cannot contain non-alphabetical characters except underscores
     * Replaces Mod Reference from SML 2.x.x versions and follows the same rules
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString Name;

    /** Display name of the mod */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString FriendlyName;

    /**
     * Version of the as it is declared in it's manifest
     * If the mod does not declare SML metadata, it's
     * semantic version is deduced from UE plugin version by using it as major
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FVersion Version;

    /** Description of the mod to be displayed in the plugin manager and mod list */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString Description;

    /** Authors of the mod, displayed to the user in the mod list and plugin manager */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString CreatedBy;

    /** Whenever this mod accepts any remote version, basically behaving as a client-/server-side only mod */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bAcceptsAnyRemoteVersion;

    /** Range of the remote versions accepted by this mod. Defaults to >=CurrentVersion, unless explicitly defined */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FVersionRange RemoteVersionRange;
};

/** Contains plugin descriptor metadata read and used by SML to provide extra functionality */
struct SML_API FSMLPluginDescriptorMetadata {

    /** Current version of the mod, following SemVer specification */
    FVersion Version;

    /** Whenever mod accepts any remote version */
    bool bAcceptsAnyRemoteVersion;

    /** Range of the accepted remote versions, by default >=Version */
    FVersionRange RemoteVersionRange;

    /** Version constraints for dependencies as specified in plugin refs */
    TMap<FString, FVersionRange> DependenciesVersions;

    /** Setups defaults for metadata from normal plugin descriptor */
    void SetupDefaults(const struct FPluginDescriptor& PluginDescriptor);

    /** Initializes metadata from plugin info object */
    void Load(const FString& PluginName, const TSharedPtr<FJsonObject> Source);
};

/** Provides access to the mod loading functionality for blueprints and allows accessing loaded mods list in implementation-agnostic manner */
UCLASS()
class SML_API UModLoadingLibrary : public UEngineSubsystem {
    GENERATED_BODY()
public:
    UModLoadingLibrary();

    /** Returns true when mod with the provided codename is loaded */
    UFUNCTION(BlueprintPure, Category = "SML|Mod Loading", meta = (BlueprintThreadSafe))
    bool IsModLoaded(const FString& Name);

    /** Returns list of all loaded mod descriptors */
    UFUNCTION(BlueprintPure, Category = "SML|Mod Loading", meta = (BlueprintThreadSafe))
    TArray<FModInfo> GetLoadedMods();

    /** Retrieves information about one particular mod by it's codename */
    UFUNCTION(BlueprintPure, Category = "SML|Mod Loading", meta = (BlueprintThreadSafe))
    bool GetLoadedModInfo(const FString& Name, FModInfo& OutModInfo);

    /** Tries to load mod icon and returns pointer to the loaded texture, or FallbackIcon if icon cannot be loaded */
    UFUNCTION(BlueprintCallable, Category = "SML|Mod Loading")
    UTexture2D* LoadModIconTexture(const FString& Name, UTexture2D* FallbackIcon);

    /** Returns the currently used SML version */
    UFUNCTION(BlueprintPure, Category = "SML|Mod Loading", meta = (BlueprintThreadSafe))
    FVersion GetModLoaderVersion() const;

    /** Returns true if development mode is currently enabled */
    UFUNCTION(BlueprintPure, Category = "SML|Mod Loading", meta = (BlueprintThreadSafe))
    bool IsDevelopmentModeEnabled() const;

    /** Returns extra attributes exposed by the mod loader implementation */
    UFUNCTION(BlueprintPure, Category = "SML|Mod Loading", meta = (BlueprintThreadSafe))
    TMap<FName, FString> GetExtraModLoaderAttributes() const;

    /** Reloads SML-related plugin metadata for active plugins */
    void ReloadPluginMetadata();

    /** Performs basic initialization of the mod loading library and basic metadata scan */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Determines whenever provided plugin should be treated as a mod. In editor, project plugins are treated as mods */
    static bool IsPluginAMod(IPlugin& Plugin);
private:
    /** Retrieves metadata for mod plugins, or creates a default metadata for engine or other plugins */
    FSMLPluginDescriptorMetadata FindMetadataOrFallback(IPlugin& Plugin);

    /** Initializes mod info object for the FactoryGame itself */
    static FModInfo CreateFactoryGameModInfo();

    /** Performs global plugin dependencies verification through all the plugins */
    void VerifyPluginDependencies();

    /** Verifies dependencies of the single plugin */
    void VerifySinglePluginDependencies(IPlugin& Plugin);

    /** Performs verification of the plugin dependencies versions, and crashes the game if they don't match */
    void VerifyPluginDependencies(IPlugin& Plugin, TArray<FString>& MismatchedDependencies);

    /** Populates mod information for the provided plugin instance */
    void PopulatePluginModInfo(IPlugin& Plugin, FModInfo& OutModInfo);

    /** Called when new plugin is created, used to preload SML metadata for it */
    void OnNewPluginCreated(IPlugin& Plugin);

    /** Makes sure metadata is loaded for the provided plugin and attempts to load it if it's not */
    void LoadMetadataForPlugin(IPlugin& Plugin);

    UPROPERTY()
    class UModIconStorage* ModIconStorage;

    TMap<FString, FSMLPluginDescriptorMetadata> PluginMetadata;
};

/** Holds mod icons and manages their loading */
UCLASS(Transient)
class SML_API UModIconStorage : public UObject {
    GENERATED_BODY()
private:
    /** Cache of loaded mod icon textures */
    UPROPERTY()
    TMap<FString, class UTexture2D*> LoadedModIcons;

    /** Blank image returned when icon cannot be loaded */
    UPROPERTY()
    UTexture2D* BlankTexture;
public:
    UModIconStorage();

    /** Loads a mod icon texture or retrieves it from cache if it has been loaded already */
    UTexture2D* FindOrLoadModIcon(const FString& PluginName, bool& bOutIsBlankTexture);
private:
    /** Actually loads mod icon */
    static UTexture2D* LoadModIcon(const FString& PluginName);
};
