#include "Configuration/ConfigManager.h"
#include "Reflection/BlueprintReflectionLibrary.h"
#include "Misc/Paths.h"
#include "Util/SemVersion.h"
#include "TimerManager.h"
#include "Configuration/RootConfigValueHolder.h"
#include "Configuration/RawFileFormat/Json/JsonRawFormatConverter.h"
#include "Engine/Engine.h"
#include "ModLoading/ModLoadingLibrary.h"
#include "Util/EngineUtil.h"

DEFINE_LOG_CATEGORY(LogConfigManager);

const TCHAR* SMLConfigModVersionField = TEXT("SML_ModVersion_DoNotChange");

void UConfigManager::ReloadModConfigurations() {
    UE_LOG(LogConfigManager, Display, TEXT("Reloading mod configurations..."));

    for (const TPair<FConfigId, FRegisteredConfigurationData>& Pair : Configurations) {
        LoadConfigurationInternal(Pair.Key, Pair.Value.RootValue, true);
    }
}

void UConfigManager::SaveConfigurationInternal(const FConfigId& ConfigId) {
    const FRegisteredConfigurationData& ConfigurationData = Configurations.FindChecked(ConfigId);

    const URootConfigValueHolder* RootValue = ConfigurationData.RootValue;
    URawFormatValue* RawFormatValue = RootValue->GetWrappedValue()->Serialize(GetTransientPackage());
    checkf(RawFormatValue, TEXT("Root RawFormatValue returned NULL for config %s"), *ConfigId.ModReference);

    // Root value should always be JsonObject, since root property is section property
    const TSharedPtr<FJsonValue> JsonValue = FJsonRawFormatConverter::ConvertToJson(RawFormatValue);
    check(JsonValue->Type == EJson::Object);
    TSharedRef<FJsonObject> UnderlyingObject = JsonValue->AsObject().ToSharedRef();

    // Record mod version so we can keep file system file schema up to date
    FModInfo ModInfo;
    UModLoadingLibrary* ModLoadingLibrary = GEngine->GetEngineSubsystem<UModLoadingLibrary>();

    if (ModLoadingLibrary->GetLoadedModInfo(ConfigId.ModReference, ModInfo)) {
        const FString ModVersion = ModInfo.Version.ToString();
        UnderlyingObject->SetStringField(SMLConfigModVersionField, ModVersion);
    }

    // Serialize resulting JSON to string
    FString JsonOutputString;
    const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonOutputString);
    FJsonSerializer::Serialize(UnderlyingObject, JsonWriter);

    // Write configuration into the file system now at the generated path
    const FString ConfigurationFilePath = GetConfigurationFilePath(ConfigId);
    // Make sure configuration directory exists
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(ConfigurationFilePath));

    if (!FFileHelper::SaveStringToFile(JsonOutputString, *ConfigurationFilePath)) {
        UE_LOG(LogConfigManager, Error, TEXT("Failed to save configuration file to %s"), *ConfigurationFilePath);
        return;
    }
    UE_LOG(LogConfigManager, Display, TEXT("Saved configuration to %s"), *ConfigurationFilePath);
}

void UConfigManager::LoadConfigurationInternal(const FConfigId& ConfigId, URootConfigValueHolder* RootConfigValueHolder, bool bSaveOnSchemaChange) {
    // Determine configuration path and try to read it to string if it exists
    const FString ConfigurationFilePath = GetConfigurationFilePath(ConfigId);

    // Check if configuration file exists, and if it doesn't, return early, optionally writing defaults
    if (!IFileManager::Get().FileExists(*ConfigurationFilePath)) {
        if (bSaveOnSchemaChange) {
            SaveConfigurationInternal(ConfigId);
        }
        return;
    }

    // Load file contents into the string for parsing
    FString JsonTextString;
    if (!FFileHelper::LoadFileToString(JsonTextString, *ConfigurationFilePath)) {
        UE_LOG(LogConfigManager, Error, TEXT("Failed to load configuration file from %s"), *ConfigurationFilePath);
        return;
    }

    // Try to parse it as valid JSON now
    const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonTextString);
    TSharedPtr<FJsonObject> JsonObject;
    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject)) {
        UE_LOG(LogConfigManager, Error, TEXT("Failed to parse configuration file %s"), *ConfigurationFilePath);
        // TODO maybe rename it and write default values instead?
        return;
    }

    // Convert JSON tree into the raw value tree and feed it to root section value
    const TSharedRef<FJsonValue> RootValue = MakeShareable(new FJsonValueObject(JsonObject));
    URawFormatValue* RawFormatValue = FJsonRawFormatConverter::ConvertToRawFormat(this, RootValue);
    RootConfigValueHolder->GetWrappedValue()->Deserialize(RawFormatValue);

    UE_LOG(LogConfigManager, Display, TEXT("Successfully loaded configuration from %s"), *ConfigurationFilePath);

    // Check that mod version matches if we are allowed to overwrite files
    FModInfo ModInfo;
    UModLoadingLibrary* ModLoadingLibrary = GEngine->GetEngineSubsystem<UModLoadingLibrary>();

    if (ModLoadingLibrary->GetLoadedModInfo(ConfigId.ModReference, ModInfo)) {
        const FString ModVersion = ModInfo.Version.ToString();
        FString FileVersion;
        if (JsonObject->HasTypedField<EJson::String>(SMLConfigModVersionField)) {
            FileVersion = JsonObject->GetStringField(SMLConfigModVersionField);
        }
        // Overwrite file if schema version doesn't match loaded mod version
        if (bSaveOnSchemaChange && FileVersion != ModVersion) {
            UE_LOG(LogConfigManager, Display, TEXT("Refreshing configuration file %s"), *ConfigurationFilePath);
            SaveConfigurationInternal(ConfigId);
        }
    }
}

void UConfigManager::FlushPendingSaves() {
    for (const FConfigId& ConfigId : PendingSaveConfigurations) {
        SaveConfigurationInternal(ConfigId);
    }
    PendingSaveConfigurations.Empty();
}

void UConfigManager::OnTimerManagerAvailable(FTimerManager* TimerManager) {
    // Setup a timer which will force all changes into filesystem every 10 seconds
    FTimerHandle OutTimerHandle;
    TimerManager->SetTimer(OutTimerHandle, FTimerDelegate::CreateUObject(this, &UConfigManager::FlushPendingSaves), 10.0f, true);
}

void UConfigManager::MarkConfigurationDirty(const FConfigId& ConfigId) {
    if (Configurations.Contains(ConfigId)) {
        PendingSaveConfigurations.AddUnique(ConfigId);
        // Make sure cached structs are synchronized with real configuration state
        ReinitializeCachedStructs(ConfigId);
    }
}

void UConfigManager::ReinitializeCachedStructs(const FConfigId& ConfigId) {
#if OPTIMIZE_FILL_CONFIGURATION_STRUCT
    const FRegisteredConfigurationData& ConfigurationData = Configurations.FindChecked(ConfigId);
    URootConfigValueHolder* RootConfigValue = ConfigurationData.RootValue;

    for (const TPair<UScriptStruct*, FReflectedObject>& Pair : ConfigurationData.CachedValues) {
        RootConfigValue->GetWrappedValue()->FillConfigStructSelf(Pair.Value);
    }
#endif
}

void UConfigManager::FillConfigurationStruct(const FConfigId& ConfigId, const FDynamicStructInfo& StructInfo) {
    FRegisteredConfigurationData* ConfigurationData = Configurations.Find(ConfigId);
    if (ConfigurationData == NULL) {
        return;
    }

#if OPTIMIZE_FILL_CONFIGURATION_STRUCT
    // If this struct type is cached, just copy it from the cache directly
    // Cached value should be up to date with configuration state as long as MarkConfigurationDirty is called properly
    const FReflectedObject* ExistingObject = ConfigurationData->CachedValues.Find(StructInfo.Struct);
    if (ExistingObject != NULL) {
        ExistingObject->CopyWrappedStruct(StructInfo.Struct, StructInfo.StructValue);
        return;
    }
#endif

    // We don't have this type of script struct in the cache, reflect it and populate
    URootConfigValueHolder* RootConfigValue = ConfigurationData->RootValue;
    const FReflectedObject StructReflection = UBlueprintReflectionLibrary::ReflectStruct(StructInfo);
    RootConfigValue->GetWrappedValue()->FillConfigStructSelf(StructReflection);

    // Copy populated reflected struct state back into original state
    StructReflection.CopyWrappedStruct(StructInfo.Struct, StructInfo.StructValue);

#if OPTIMIZE_FILL_CONFIGURATION_STRUCT
    // Store cached struct reflection in the cache
    ConfigurationData->CachedValues.Add(StructInfo.Struct, StructReflection);
#endif
}

UUserWidget* UConfigManager::CreateConfigurationWidget(const FConfigId& ConfigId, UUserWidget* Outer) {
    FRegisteredConfigurationData* ConfigurationData = Configurations.Find(ConfigId);
    if (ConfigurationData == NULL) {
        return NULL;
    }

    UConfigPropertySection* RootValue = ConfigurationData->RootValue->GetWrappedValue();
    return RootValue->CreateEditorWidget(Outer);
}

void UConfigManager::ReplaceConfigurationClass(FRegisteredConfigurationData* ExistingData, TSubclassOf<UModConfiguration> NewConfiguration) {
    // Dump current configuration data into temporary raw format value chain
    URawFormatValue* TempDataObject = ExistingData->RootValue->GetWrappedValue()->Serialize(this);

    // Create new root section value from new configuration class
    URootConfigValueHolder* RootConfigValueHolder = ExistingData->RootValue;

    // Replace wrapped configuration section value with new root section, replace old configuration class
    RootConfigValueHolder->UpdateWrappedValue(NewConfiguration.GetDefaultObject()->RootSection);
    ExistingData->ConfigurationClass = NewConfiguration;

    // Populate new configuration with data from previous one
    RootConfigValueHolder->GetWrappedValue()->Deserialize(TempDataObject);

    // Refresh all cached struct values with new data
    ReinitializeCachedStructs(ExistingData->ConfigId);

    // Force configuration save into filesystem with new schema
    SaveConfigurationInternal(ExistingData->ConfigId);
}

bool IsCompatibleConfigurationClassChange(UClass* OldConfigurationClass, UClass* NewConfigurationClass) {
    // If class object is the same, this change is compatible
    if (OldConfigurationClass == NewConfigurationClass) {
        return true;
    }

    // If never version of old configuration class exists, and it is new class, replace is compatible
    if (OldConfigurationClass->HasAnyClassFlags(EClassFlags::CLASS_NewerVersionExists)) {
        UBlueprint* GeneratedByBP = Cast<UBlueprint>(OldConfigurationClass->ClassGeneratedBy);
        if (GeneratedByBP) {
            UClass* NewBlueprintClass = GeneratedByBP->GeneratedClass;
            return NewBlueprintClass == NewConfigurationClass;
        }
    }

    // Otherwise, replace is not compatible
    return false;
}

void UConfigManager::RegisterModConfiguration(TSubclassOf<UModConfiguration> Configuration) {
    checkf(Configuration, TEXT("Attempt to register NULL configuration"));
    UE_LOG(LogSatisfactoryModLoader, Log, TEXT("Registering configuration %s"), *Configuration->GetPathName());

    UModConfiguration* ClassDefaultObject = Configuration.GetDefaultObject();
    const FConfigId ConfigId = ClassDefaultObject->ConfigId;
    FRegisteredConfigurationData* ExistingData = Configurations.Find(ConfigId);

    // Registration already exists for this configuration ID
    // It can totally happen since we are an engine level subsystem and in editor multiple PIE launches can happen
    if (ExistingData != NULL) {
        // Check if replacement is compatible though, and if it's not, log warning
        if (!IsCompatibleConfigurationClassChange(ExistingData->ConfigurationClass, Configuration)) {
            UE_LOG(LogConfigManager, Warning, TEXT("Replacing configuration %s:%s with new class %s"), *ConfigId.ModReference, *ConfigId.ConfigCategory, *Configuration->GetPathName());
        }
        // Run configuration replacement schedule
        ReplaceConfigurationClass(ExistingData, Configuration);
        return;
    }

    // Create root value and wrap it into config root handling marking config dirty
    URootConfigValueHolder* RootConfigValueHolder = NewObject<URootConfigValueHolder>(this);
    RootConfigValueHolder->SetupRootValue(this, ConfigId);
    RootConfigValueHolder->UpdateWrappedValue(Configuration.GetDefaultObject()->RootSection);

    // Register configuration inside all of the internal properties
    Configurations.Add(ConfigId, FRegisteredConfigurationData{ConfigId, Configuration, RootConfigValueHolder});

    // Reload configuration from the disk once it has been registered
    LoadConfigurationInternal(ConfigId, RootConfigValueHolder, true);
}

TSubclassOf<UModConfiguration> UConfigManager::GetConfigurationById(const FConfigId& ConfigId) const {
    const FRegisteredConfigurationData* ConfigurationData = Configurations.Find(ConfigId);
    return ConfigurationData ? ConfigurationData->ConfigurationClass : NULL;
}

UConfigPropertySection* UConfigManager::GetConfigurationRootSection(const FConfigId& ConfigId) const {
    const FRegisteredConfigurationData* ConfigurationData = Configurations.Find(ConfigId);
    return ConfigurationData ? ConfigurationData->RootValue->GetWrappedValue() : NULL;
}

void UConfigManager::Initialize(FSubsystemCollectionBase& Collection) {
    // Subscribe to exit event so we make sure that pending saves are written to filesystem
    FCoreDelegates::OnPreExit.AddUObject(this, &UConfigManager::FlushPendingSaves);
    // Subscribe to timer manager availability delegate to be able to do periodic auto-saves
    FEngineUtil::DispatchWhenTimerManagerIsReady(TBaseDelegate<void, FTimerManager*>::CreateUObject(this, &UConfigManager::OnTimerManagerAvailable));
}

FString UConfigManager::GetConfigurationFolderPath() {
    return FPaths::ProjectDir() + TEXT("Configs/");
}

FString UConfigManager::GetConfigurationFilePath(const FConfigId& ConfigId) {
    const FString ConfigDirectory = GetConfigurationFolderPath();
    if (ConfigId.ConfigCategory == TEXT("")) {
        // Category is empty, that means mod has only one configuration file
        return ConfigDirectory + FString::Printf(TEXT("%s.cfg"), *ConfigId.ModReference);
    }
    // We have a category, so mod reference is a folder and category is a file name
    return ConfigDirectory + FString::Printf(TEXT("%s/%s.cfg"), *ConfigId.ModReference, *ConfigId.ConfigCategory);
}
