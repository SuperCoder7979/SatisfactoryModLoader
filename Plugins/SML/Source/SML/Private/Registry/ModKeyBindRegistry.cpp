#include "Registry/ModKeyBindRegistry.h"

#include "FGGameUserSettings.h"
#include "FGOptionsSettings.h"
#include "GameFramework/InputSettings.h"

void UModKeyBindRegistry::RegisterModKeyBind(const FString& ModReference, FInputActionKeyMapping KeyMapping, const FText& DisplayName) {
    UInputSettings* InputSettings = UInputSettings::GetInputSettings();
    UFGOptionsSettings* OptionsSettings = GetMutableDefault<UFGOptionsSettings>();
    const FString ModPrefix = FString::Printf(TEXT("%s."), *ModReference);

    // Ensure that we are prefixed by ModReference to allow unique identification
    const FString ActionName = KeyMapping.ActionName.ToString();
    checkf(ActionName.StartsWith(ModPrefix), TEXT("RegisterModKeyBind called with ActionName not being prefixed by ModReference"));

    // Try to find changed user settings for the key bind
    UFGGameUserSettings* UserSettings = UFGGameUserSettings::GetFGGameUserSettings();
    for (const FFGKeyMapping& KeyMap : UserSettings->GetKeyMappings()) {
        if (KeyMap.IsAxisMapping) continue;
        if (KeyMap.ActionKeyMapping.ActionName == KeyMapping.ActionName) {
            KeyMapping = KeyMap.ActionKeyMapping;
            break;
        }
    }

    // Check for uniqueness. We want mapping registered only one time
    TArray<FInputActionKeyMapping> MappingsAlreadyRegistered;
    InputSettings->GetActionMappingByName(KeyMapping.ActionName, MappingsAlreadyRegistered);
    if (MappingsAlreadyRegistered.Contains(KeyMapping)) {
        return;
    }

    // If we already have non-gamepad/gamepad mapping registered, don't register this one
    // This is because FactoryGame currently can only differentiate 2 mappings with same action name currently:
    // One should be bound to gamepad (and be not editable in controls), and other should be not-gamepad,
    // but editable in control options. So we only allow 2 mappings at most, and they should have different types
    const bool bIsGamePadKey = KeyMapping.Key.IsGamepadKey();
    for (const FInputActionKeyMapping& OtherMapping : MappingsAlreadyRegistered) {
        const bool bIsOtherGamePadKey = OtherMapping.Key.IsGamepadKey();
        if (bIsGamePadKey == bIsOtherGamePadKey) return; // Disallow registering 2 mappings with same type
    }

    // Either we don't have registered mapping, or it is of different type at this point
    // We can add it now, and calling AddActionMapping will also update all active PlayerInput objects
    InputSettings->AddActionMapping(KeyMapping);

    // Only register display name for non-gamepad mappings, because
    // FactoryGame option slider currently only supports 1 key per 1 action, and it
    // should be non-gamepad action key mapping
    if (!bIsGamePadKey) {
        FActionMappingDisplayName DisplayNameMapping;
        DisplayNameMapping.ActionMappingName = KeyMapping.ActionName;
        DisplayNameMapping.DisplayName = DisplayName;
        OptionsSettings->mActionBindingsDisplayNames.Add(DisplayNameMapping);
    }
}

void PerformChecksForModAxisBindings(const FInputAxisKeyMapping& PositiveAxisMapping, const FInputAxisKeyMapping& NegativeAxisMapping) {
    const FString PositiveAxisName = PositiveAxisMapping.AxisName.ToString();
    const FString NegativeAxisName = NegativeAxisMapping.AxisName.ToString();
    checkf(PositiveAxisName == NegativeAxisName, TEXT("RegisterModAxisBind called with different axis names for positive and negative mappings"));
    checkf(PositiveAxisMapping.Scale > 0, TEXT("PositiveAxisMapping should have positive scale"));
    checkf(NegativeAxisMapping.Scale < 0, TEXT("NegativeAxisMapping should have negative scale"));
    checkf(PositiveAxisMapping.Key.IsGamepadKey() == NegativeAxisMapping.Key.IsGamepadKey(), TEXT("Negative and Positive mappings should be same type"));
}

void UModKeyBindRegistry::RegisterModAxisBind(const FString& ModReference, FInputAxisKeyMapping PositiveAxisMapping, FInputAxisKeyMapping NegativeAxisMapping, const FText& PositiveDisplayName, const FText& NegativeDisplayName) {
    UInputSettings* InputSettings = UInputSettings::GetInputSettings();
    UFGOptionsSettings* OptionsSettings = GetMutableDefault<UFGOptionsSettings>();
    const FString ModPrefix = FString::Printf(TEXT("%s."), *ModReference);

    // Just like with action mapping, check that both axis names start with mod prefix
    const FString AxisName = PositiveAxisMapping.AxisName.ToString();
    checkf(AxisName.StartsWith(ModPrefix), TEXT("RegisterModAxisBind called with AxisName not being prefixed by ModReference"));
    PerformChecksForModAxisBindings(PositiveAxisMapping, NegativeAxisMapping);

    // Try to find changed user settings for the key bind
    UFGGameUserSettings* UserSettings = UFGGameUserSettings::GetFGGameUserSettings();
    for (const FFGKeyMapping& KeyMap : UserSettings->GetKeyMappings()) {
        if (KeyMap.IsAxisMapping) continue;
        if (KeyMap.AxisKeyMapping.AxisName == PositiveAxisMapping.AxisName) {
            if (KeyMap.AxisKeyMapping.Scale > 0) PositiveAxisMapping = KeyMap.AxisKeyMapping;
            if (KeyMap.AxisKeyMapping.Scale < 0) NegativeAxisMapping = KeyMap.AxisKeyMapping;
        }
    }

    // Ensure we don't have duplicate axis mappings already registered
    TArray<FInputAxisKeyMapping> MappingsAlreadyRegistered;
    InputSettings->GetAxisMappingByName(PositiveAxisMapping.AxisName, MappingsAlreadyRegistered);
    if (MappingsAlreadyRegistered.Contains(PositiveAxisMapping) ||
        MappingsAlreadyRegistered.Contains(NegativeAxisMapping)) {
        return;
    }

    // Ensure we don't have same type of axis mappings registered
    const bool bIsGamePadKey = PositiveAxisMapping.Key.IsGamepadKey();
    for (const FInputAxisKeyMapping& OtherMapping : MappingsAlreadyRegistered) {
        const bool bIsOtherGamePadKey = OtherMapping.Key.IsGamepadKey();
        if (bIsGamePadKey == bIsOtherGamePadKey) return; // Disallow registering 2 mappings with same type
    }

    // Register both axis bindings
    InputSettings->AddAxisMapping(PositiveAxisMapping);
    InputSettings->AddAxisMapping(NegativeAxisMapping);

    // Only register display name for non-gamepad mappings, same reason as for keys
    if (!bIsGamePadKey) {
        FAxisMappingDisplayName DisplayName;
        DisplayName.AxisMappingName = PositiveAxisMapping.AxisName;
        DisplayName.DisplayNamePositiveScale = PositiveDisplayName;
        DisplayName.DisplayNameNegativeScale = NegativeDisplayName;
        OptionsSettings->mAxisBindingsDisplayNames.Add(DisplayName);
    }
}
