#include "ModLoading/ModLoadingLibrary.h"
#include "PluginDescriptor.h"
#include "SatisfactoryModLoader.h"
#include "Interfaces/IPluginManager.h"
#include "Util/ImageLoadingUtil.h"
#include "Json.h"

// We only want to enforce plugin dependency versions outside of the editor
#define ENFORCE_PLUGIN_DEPENDENCY_VERSIONS !WITH_EDITOR

void FSMLPluginDescriptorMetadata::SetupDefaults(const FPluginDescriptor& PluginDescriptor) {
    this->Version = FVersion(PluginDescriptor.Version, 0, 0);
    this->bAcceptsAnyRemoteVersion = false;
    this->RemoteVersionRange = FVersionRange::CreateRangeWithMinVersion(Version);
}

void FSMLPluginDescriptorMetadata::Load(const FString& PluginName, const TSharedPtr<FJsonObject> Source) {

    // Try to parse SemVersion metadata to get proper semantic version of the plugin
    FString SemanticVersion;
    if (Source->TryGetStringField(TEXT("SemVersion"), SemanticVersion)) {

        FVersion ResultVersion;
        FString VersionParseError;
        const bool bParseSuccess = ResultVersion.ParseVersion(SemanticVersion, VersionParseError);

        if (bParseSuccess) {
            this->Version = ResultVersion;
            this->RemoteVersionRange = FVersionRange::CreateRangeWithMinVersion(Version);
            this->bAcceptsAnyRemoteVersion = false;

        } else {
            UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Plugin/Mod %s has invalid Semantic Version value: '%s': %s"), *PluginName, *SemanticVersion, *VersionParseError);
        }
    } else {
        UE_LOG(LogSatisfactoryModLoader, Warning, TEXT("Plugin/Mod %s does not specify 'SemVersion' field, falling back to UE Version"), *PluginName);
    }

    // Try to parse AcceptsAnyRemoteVersion boolean, indicating policy when remote is missing mod
    Source->TryGetBoolField(TEXT("AcceptsAnyRemoteVersion"), bAcceptsAnyRemoteVersion);

    // Try to parse RemoteVersionRange
    FString RemoteVersionRangeString;
    if (Source->TryGetStringField(TEXT("RemoteVersionRange"), RemoteVersionRangeString)) {

        FVersionRange VersionRange;
        FString VersionRangeError;

        if (VersionRange.ParseVersionRange(RemoteVersionRangeString, VersionRangeError)) {
            this->RemoteVersionRange = VersionRange;
        } else {
            UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Plugin %s has invalid Remote Version Range value: %s: %s"), *PluginName, *RemoteVersionRangeString, *VersionRangeError);
        }
        if (bAcceptsAnyRemoteVersion) {
            UE_LOG(LogSatisfactoryModLoader, Warning, TEXT("Plugin %s specifies remote version range while also having acceptAnyRemoteVersion set"), *PluginName);
        }
    }

    // Loop plugins specified in the plugin manifest and detect version predicate inside of them
    const TArray<TSharedPtr<FJsonValue>>* PluginsArray;
    if (Source->TryGetArrayField(TEXT("Plugins"), PluginsArray)) {
        for (const TSharedPtr<FJsonValue>& Item : *PluginsArray) {
            const TSharedPtr<FJsonObject>* ObjectPtr;

            if (Item.IsValid() && Item->TryGetObject(ObjectPtr)) {
                const FString DependencyName = (*ObjectPtr)->GetStringField(TEXT("Name"));
                FString DependencyVersionRangeString;
                if ((*ObjectPtr)->TryGetStringField(TEXT("SemVersion"), DependencyVersionRangeString)) {

                    FVersionRange DependencyVersionRange;
                    FString DependencyErrorMessage;
                    if (DependencyVersionRange.ParseVersionRange(DependencyVersionRangeString, DependencyErrorMessage)) {
                        this->DependenciesVersions.Add(DependencyName, DependencyVersionRange);
                    } else {
                        UE_LOG(
                            LogSatisfactoryModLoader,
                            Error,
                            TEXT("Plugin %s has invalid dependency '%s' version range '%s': %s"),
                            *PluginName,
                            *DependencyName,
                            *DependencyVersionRangeString,
                            *DependencyErrorMessage
                        );
                    }
                }
            }

        }
    }
}

UModLoadingLibrary::UModLoadingLibrary() {
    this->ModIconStorage = CreateDefaultSubobject<UModIconStorage>(TEXT("ModIconStorage"));
}

bool UModLoadingLibrary::IsModLoaded(const FString& Name) {
    FModInfo LoadedModInfo;
    return GetLoadedModInfo(Name, LoadedModInfo);
}

TArray<FModInfo> UModLoadingLibrary::GetLoadedMods() {
    TArray<FModInfo> OutModInfoList;
    OutModInfoList.Add(CreateFactoryGameModInfo());

    const TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins) {
        if (IsPluginAMod(Plugin.Get())) {
            PopulatePluginModInfo(Plugin.Get(), OutModInfoList.AddDefaulted_GetRef());
        }
    }

    return OutModInfoList;
}

bool UModLoadingLibrary::GetLoadedModInfo(const FString& Name, FModInfo& OutModInfo) {
    if (Name == FACTORYGAME_MOD_NAME) {
        OutModInfo = CreateFactoryGameModInfo();
    }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Name);

    if (Plugin.IsValid() && Plugin->IsEnabled() && IsPluginAMod(*Plugin)) {
        PopulatePluginModInfo(*Plugin.Get(), OutModInfo);
        return true;
    }
    return false;
}

void UModLoadingLibrary::Initialize(FSubsystemCollectionBase& Collection) {
    // Add some callbacks to handle plugins being mounted later in the lifecycle gracefully
    IPluginManager::Get().OnNewPluginCreated().AddUObject(this, &UModLoadingLibrary::OnNewPluginCreated);
    IPluginManager::Get().OnNewPluginMounted().AddUObject(this, &UModLoadingLibrary::OnNewPluginCreated);

    // Initialize metadata and check dependencies for plugins that have already been loaded
    ReloadPluginMetadata();
    VerifyPluginDependencies();
}

FSMLPluginDescriptorMetadata UModLoadingLibrary::FindMetadataOrFallback(IPlugin& Plugin) {
    const FSMLPluginDescriptorMetadata* PluginDescriptorMetadata = PluginMetadata.Find(Plugin.GetName());

    // Metadata already exists, just return it normally
    if (PluginDescriptorMetadata) {
        return *PluginDescriptorMetadata;
    }

    // Metadata does not exist, this should be non-mod module. Fallback to defaults
    FSMLPluginDescriptorMetadata DefaultMetadata{};
    DefaultMetadata.SetupDefaults(Plugin.GetDescriptor());
    return DefaultMetadata;
}

bool UModLoadingLibrary::IsPluginAMod(IPlugin& Plugin) {
    // Mod plugins are always considered mods
    if (Plugin.GetType() == EPluginType::Mod) {
        return true;
    }
    // Project plugins are considered mods too when we're built with editor
#if WITH_EDITOR
    if (Plugin.GetType() == EPluginType::Project) {
        return true;
    }
#endif
    return false;
}

FModInfo UModLoadingLibrary::CreateFactoryGameModInfo() {
    const int64 ChangelistNum = FEngineVersion::Current().GetChangelist();
    FModInfo ResultModInfo{};
    ResultModInfo.Name = FACTORYGAME_MOD_NAME;
    ResultModInfo.Version = FVersion(ChangelistNum, 0, 0);

    ResultModInfo.FriendlyName = TEXT("Satisfactory");
    ResultModInfo.CreatedBy = TEXT("Coffee Stain Studios");
    ResultModInfo.bAcceptsAnyRemoteVersion = true;
    return ResultModInfo;
}

void UModLoadingLibrary::VerifyPluginDependencies() {
#if ENFORCE_PLUGIN_DEPENDENCY_VERSIONS
    TArray<FString> MismatchedDependencies;

    const TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins) {
        if (IsPluginAMod(Plugin.Get())) {
            VerifyPluginDependencies(Plugin.Get(), MismatchedDependencies);
        }
    }

    if (MismatchedDependencies.Num()) {
        const FString ErrorList = FString::Join(MismatchedDependencies, TEXT("\n"));
        UE_LOG(LogSatisfactoryModLoader, Fatal, TEXT("Found mismatched dependencies versions in the environment. Loading cannot continue: \n%s"), *ErrorList);
    }
#endif
}

void UModLoadingLibrary::VerifySinglePluginDependencies(IPlugin& Plugin) {
#if ENFORCE_PLUGIN_DEPENDENCY_VERSIONS
    TArray<FString> MismatchedDependencies;
    VerifyPluginDependencies(Plugin, MismatchedDependencies);

    if (MismatchedDependencies.Num()) {
        const FString ErrorList = FString::Join(MismatchedDependencies, TEXT("\n"));
        UE_LOG(LogSatisfactoryModLoader, Fatal, TEXT("Found mismatched dependencies versions in the environment. Loading cannot continue: \n%s"), *ErrorList);
    }
#endif
}

void UModLoadingLibrary::VerifyPluginDependencies(IPlugin& Plugin, TArray<FString>& MismatchedDependencies) {
    // Plugin metadata should be already loaded at this point
    const FPluginDescriptor& PluginDescriptor = Plugin.GetDescriptor();
    const FSMLPluginDescriptorMetadata& PluginDescriptorMetadata = PluginMetadata.FindChecked(Plugin.GetName());
    IPluginManager& PluginManager = IPluginManager::Get();

    for (const FPluginReferenceDescriptor& PluginDependency : PluginDescriptor.Plugins) {
        const TSharedPtr<IPlugin> DependencyPlugin = PluginManager.FindPlugin(PluginDependency.Name);

        // Skip missing optional dependencies, plugin can only be null if it was optional
        if (!DependencyPlugin.IsValid()) {
            continue;
        }

        // Version constraints are not specified for this plugin, just skip it
        const FVersionRange* VersionRange = PluginDescriptorMetadata.DependenciesVersions.Find(DependencyPlugin->GetName());
        if (VersionRange == NULL) {
            continue;
        }

        // Retrieve metadata for the dependency
        FSMLPluginDescriptorMetadata DependencyMetadata = FindMetadataOrFallback(*DependencyPlugin.Get());

        // Check versions, if they do not match, crash the game
        if (!VersionRange->Matches(DependencyMetadata.Version)) {
            const FString Message = FString::Printf(
                TEXT("Plugin %s requires %s version matching %s (received: %s)"),
                *Plugin.GetName(),
                *DependencyPlugin->GetName(),
                *VersionRange->ToString(),
                *DependencyMetadata.Version.ToString()
            );
            MismatchedDependencies.Add(Message);
        }
    }
}

void UModLoadingLibrary::PopulatePluginModInfo(IPlugin& Plugin, FModInfo& OutModInfo) {
    // Make sure plugin metadata is loaded
    LoadMetadataForPlugin(Plugin);

    // Retrieve plugin descriptor and metadata and use it to populate mod info
    const FPluginDescriptor& PluginDescriptor = Plugin.GetDescriptor();
    const FSMLPluginDescriptorMetadata& PluginDescriptorMetadata = PluginMetadata.FindChecked(Plugin.GetName());

    OutModInfo.Name = Plugin.GetName();
    OutModInfo.Version = PluginDescriptorMetadata.Version;

    OutModInfo.FriendlyName = PluginDescriptor.FriendlyName;
    OutModInfo.Description = PluginDescriptor.Description;
    OutModInfo.CreatedBy = PluginDescriptor.CreatedBy;

    OutModInfo.bAcceptsAnyRemoteVersion = PluginDescriptorMetadata.bAcceptsAnyRemoteVersion;
    OutModInfo.RemoteVersionRange = PluginDescriptorMetadata.RemoteVersionRange;
}

void UModLoadingLibrary::OnNewPluginCreated(IPlugin& Plugin) {
    if (Plugin.IsEnabled() && IsPluginAMod(Plugin)) {
        // Only perform metadata loading and dependencies verification if plugin hasn't been checked before
        if (!PluginMetadata.Contains(Plugin.GetName())) {
            LoadMetadataForPlugin(Plugin);
            VerifySinglePluginDependencies(Plugin);
        }
    }
}

void UModLoadingLibrary::ReloadPluginMetadata() {
    const TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins) {
        if (IsPluginAMod(Plugin.Get())) {
            LoadMetadataForPlugin(Plugin.Get());
        }
    }
}

TSharedPtr<FJsonObject> ParsePluginDescriptorFile(IPlugin& Plugin) {
    const FString PluginDescriptorFilePath = Plugin.GetDescriptorFileName();

    FString FileContents;
    if (!FFileHelper::LoadFileToString(FileContents, *PluginDescriptorFilePath)) {
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Failed to open descriptor file %s for plugin %s"), *PluginDescriptorFilePath, *Plugin.GetName());
        return NULL;
    }

    const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FileContents);
    TSharedPtr<FJsonObject> OutObject;
    if (!FJsonSerializer::Deserialize(JsonReader, OutObject)) {
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Failed to parse descriptor file %s for plugin %s, invalid json"), *PluginDescriptorFilePath, *Plugin.GetName());
        return NULL;
    }

    return OutObject;
}

void UModLoadingLibrary::LoadMetadataForPlugin(IPlugin& Plugin) {
    if (Plugin.IsEnabled() && IsPluginAMod(Plugin) && !PluginMetadata.Contains(Plugin.GetName())) {

        const FPluginDescriptor& PluginDescriptor = Plugin.GetDescriptor();
        FSMLPluginDescriptorMetadata PluginDescriptorMetadata{};
        PluginDescriptorMetadata.SetupDefaults(PluginDescriptor);

        const TSharedPtr<FJsonObject> PluginDescriptorObject = ParsePluginDescriptorFile(Plugin);
        if (PluginDescriptorObject.IsValid()) {
            PluginDescriptorMetadata.Load(Plugin.GetName(), PluginDescriptorObject);
        }

        this->PluginMetadata.Add(Plugin.GetName(), PluginDescriptorMetadata);
    }
}

FVersion UModLoadingLibrary::GetModLoaderVersion() const {
    return FSatisfactoryModLoader::GetModLoaderVersion();
}

bool UModLoadingLibrary::IsDevelopmentModeEnabled() const {
    return FSatisfactoryModLoader::GetSMLConfiguration().bDevelopmentMode;
}

TMap<FName, FString> UModLoadingLibrary::GetExtraModLoaderAttributes() const {
    return FSatisfactoryModLoader::GetExtraAttributes();
}

UModIconStorage::UModIconStorage() {
    this->BlankTexture = UTexture2D::CreateTransient(256, 256);
}

UTexture2D* UModIconStorage::FindOrLoadModIcon(const FString& PluginName, bool& bOutIsBlankTexture) {
    // Icon is already loaded and cached
    if (LoadedModIcons.Contains(PluginName)) {
        UTexture2D* LoadedIcon = LoadedModIcons.FindChecked(PluginName);
        bOutIsBlankTexture = LoadedIcon == BlankTexture;
    }

    UTexture2D* ActuallyLoadedTexture = LoadModIcon(PluginName);
    if (ActuallyLoadedTexture == NULL) {
        ActuallyLoadedTexture = BlankTexture;
    }

    LoadedModIcons.Add(PluginName, ActuallyLoadedTexture);
    bOutIsBlankTexture = ActuallyLoadedTexture == BlankTexture;
    return ActuallyLoadedTexture;
}

UTexture2D* UModIconStorage::LoadModIcon(const FString& PluginName) {
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);

    // Make sure plugin is valid, enabled and it's actually a mod
    if (!Plugin.IsValid() || !Plugin->IsEnabled() || !UModLoadingLibrary::IsPluginAMod(*Plugin)) {
        return NULL;
    }

    // Plugins have a fixed icon path, which is PluginRoot/Resources/Icon128.png
    const FString Icon128FilePath = Plugin->GetBaseDir() / TEXT("Resources/Icon128.png");

    FString OutErrorMessage;
    UTexture2D* LoadedModIcon = FImageLoadingUtil::LoadImageFromFile(*Icon128FilePath, OutErrorMessage);

    // Failed to load mod icon, fallback to default
    if (LoadedModIcon == NULL) {
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Failed to load icon for plugin %s at file %s: %s"), *PluginName, *Icon128FilePath, *OutErrorMessage);
        return NULL;
    }
    return LoadedModIcon;
}

UTexture2D* UModLoadingLibrary::LoadModIconTexture(const FString& ModReference, UTexture2D* FallbackIcon) {
    bool bIsBlankTexture = false;
    UTexture2D* ModIconTexture = ModIconStorage->FindOrLoadModIcon(ModReference, bIsBlankTexture);
    if (bIsBlankTexture && FallbackIcon) {
        return FallbackIcon;
    }
    return ModIconTexture;
}
