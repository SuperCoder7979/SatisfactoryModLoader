#include "Module/GameInstanceModule.h"
#include "Configuration/ConfigManager.h"
#include "Tooltip/ItemTooltipSubsystem.h"
#include "Registry/ModKeyBindRegistry.h"
#include "Engine/Engine.h"

UGameInstance* UGameInstanceModule::GetGameInstance() const {
    // Game Instance module hierarchy is built on top of UGameInstanceSubsystem,
    // which has UGameInstance itself as outer object
    // so retrieving game instance pointer is rather trivial: walk outer chain until there is an UFGGameInstance
    return GetTypedOuter<UGameInstance>();
}

UWorld* UGameInstanceModule::GetWorld() const {
    // Return nullptr on class default object to trick ImplementsGetWorld()
    if (HasAllFlags(RF_ClassDefaultObject)) {
        return nullptr;
    }

    // Otherwise we retrieve world context from game instance
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetWorld() : NULL;
}

void UGameInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
    // Register default content before calling blueprint event logic
    if (Phase == ELifecyclePhase::INITIALIZATION) {
        RegisterDefaultContent();
    }

    Super::DispatchLifecycleEvent(Phase);
}

void UGameInstanceModule::RegisterDefaultContent() {
    // Register default content
    UGameInstance* GameInstance = GetGameInstance();
    UConfigManager* ConfigManager = GameInstance->GetEngine()->GetEngineSubsystem<UConfigManager>();
    UItemTooltipSubsystem* ItemTooltipSubsystem = GameInstance->GetSubsystem<UItemTooltipSubsystem>();

    const FString OwnerModReferenceString = GetOwnerModReference().ToString();

    // Register mod configurations first
    for (const TSubclassOf<UModConfiguration>& Configuration : ModConfigurations) {
        ConfigManager->RegisterModConfiguration(Configuration);
    }

    for (UClass* GlobalTooltipProvider : GlobalItemTooltipProviders) {
        ItemTooltipSubsystem->RegisterGlobalTooltipProvider(OwnerModReferenceString, GlobalTooltipProvider->GetDefaultObject());
    }

    // Key Bindings and Axis Bindings
    for (const FModKeyBindingInfo& KeyBindingInfo : ModKeyBindings) {
        // Because action binding editor UI is stupid and you cannot set action name directly apparently...
        FInputActionKeyMapping FixedKeyMapping = KeyBindingInfo.KeyMapping;
        FixedKeyMapping.ActionName = KeyBindingInfo.ActionName;

        // Register fixed key mapping with proper action name now
        UModKeyBindRegistry::RegisterModKeyBind(OwnerModReferenceString, FixedKeyMapping, KeyBindingInfo.DisplayName);
    }

    for (const FModAxisBindingInfo& AxisBindingInfo : ModAxisBindings) {
        // Same problem as with action binding editor - see comment above
        FInputAxisKeyMapping FixedPositiveAxisMapping = AxisBindingInfo.PositiveAxisMapping;
        FInputAxisKeyMapping FixedNegativeAxisMapping = AxisBindingInfo.NegativeAxisMapping;

        FixedPositiveAxisMapping.AxisName = AxisBindingInfo.AxisName;
        FixedNegativeAxisMapping.AxisName = AxisBindingInfo.AxisName;

        // Register fixed axis mapping with proper action names now
        UModKeyBindRegistry::RegisterModAxisBind(OwnerModReferenceString, FixedPositiveAxisMapping, FixedNegativeAxisMapping, AxisBindingInfo.PositiveAxisDisplayName, AxisBindingInfo.NegativeAxisDisplayName);
    }
}
