#include "Util/RuntimeBlueprintFunctionLibrary.h"

UClass* URuntimeBlueprintFunctionLibrary::FindClassByName(FString ClassNameInput) {
    // prevent crash from wrong user Input
    if (ClassNameInput.Len() == 0) return nullptr;

    const TCHAR* ClassName = *ClassNameInput;
    UObject* ClassPackage = ANY_PACKAGE;

    if (UClass* Result = FindObject<UClass>(ANY_PACKAGE, ClassName, false)) return Result;

    if (UObjectRedirector* RenamedClassRedirector = FindObject<UObjectRedirector>(ANY_PACKAGE, ClassName, true)) return CastChecked<UClass>(RenamedClassRedirector->DestinationObject);

    return nullptr;
}

bool URuntimeBlueprintFunctionLibrary::IsEditor() {
#if WITH_EDITOR
    return true;
#else
    return false;
#endif
}

FName URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToName(UConfigProperty* Property) {
    if (Cast<UConfigPropertyString>(Property)) {
        UConfigPropertyString* Obj = Cast<UConfigPropertyString>(Property);
        return FName(Obj->Value);
    } else if (Cast<UConfigPropertyBool>(Property)) {
        UConfigPropertyBool* Obj = Cast<UConfigPropertyBool>(Property);
        return FName(FString::FromInt(int32(Obj->Value)));
    } else if (Cast<UConfigPropertyFloat>(Property)) {
        UConfigPropertyFloat* Obj = Cast<UConfigPropertyFloat>(Property);
        return FName(FString::SanitizeFloat(Obj->Value));
    } else if (Cast<UConfigPropertyInteger>(Property)) {
        UConfigPropertyInteger* Obj = Cast<UConfigPropertyInteger>(Property);
        return FName(FString::FromInt(int32(Obj->Value)));
    } else if (Cast<UConfigPropertyArray>(Property)) {
        return FName(TEXT("Array"));
    } else if (Cast<UConfigPropertyArray>(Property)) {
        return FName(TEXT("Array"));
    } else return FName(TEXT("NULL"));
}

FText URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToText(UConfigProperty* Property) {
    if (Cast<UConfigPropertyString>(Property)) {
        UConfigPropertyString* Obj = Cast<UConfigPropertyString>(Property);
        return FText::FromString(Obj->Value);
    } else if (Cast<UConfigPropertyBool>(Property)) {
        UConfigPropertyBool* Obj = Cast<UConfigPropertyBool>(Property);
        return FText::FromString(FString::FromInt(int32(Obj->Value)));
    } else if (Cast<UConfigPropertyFloat>(Property)) {
        UConfigPropertyFloat* Obj = Cast<UConfigPropertyFloat>(Property);
        return FText::FromString(FString::SanitizeFloat(Obj->Value));
    } else if (Cast<UConfigPropertyInteger>(Property)) {
        UConfigPropertyInteger* Obj = Cast<UConfigPropertyInteger>(Property);
        return FText::FromString(FString::FromInt(int32(Obj->Value)));
    } else if (Cast<UConfigPropertyArray>(Property)) {
        return FText::FromString(TEXT("Array"));
    } else if (Cast<UConfigPropertySection>(Property)) {
        return FText::FromString(TEXT("OBJECT"));
    } else return FText::FromString(TEXT("NULL"));
}

FString URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToString(UConfigProperty* Property) {
    if (Cast<UConfigPropertyString>(Property)) {
        UConfigPropertyString* Obj = Cast<UConfigPropertyString>(Property);
        return Obj->Value;
    } else if (Cast<UConfigPropertyBool>(Property)) {
        UConfigPropertyBool* Obj = Cast<UConfigPropertyBool>(Property);
        return FString::FromInt(int32(Obj->Value));
    } else if (Cast<UConfigPropertyFloat>(Property)) {
        UConfigPropertyFloat* Obj = Cast<UConfigPropertyFloat>(Property);
        return FString::SanitizeFloat(Obj->Value);
    } else if (Cast<UConfigPropertyInteger>(Property)) {
        UConfigPropertyInteger* Obj = Cast<UConfigPropertyInteger>(Property);
        return FString::FromInt(int32(Obj->Value));
    } else if (Cast<UConfigPropertyArray>(Property)) {
        return TEXT("Array");
    } else if (Cast<UConfigPropertySection>(Property)) {
        return TEXT("OBJECT");
    } else return TEXT("NULL");
};

float URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToFloat(UConfigProperty* Property) {
    if (Cast<UConfigPropertyFloat>(Property)) {
        UConfigPropertyFloat* Obj = Cast<UConfigPropertyFloat>(Property);
        return Obj->Value;
    } else if (Cast<UConfigPropertyBool>(Property)) {
        UConfigPropertyBool* Obj = Cast<UConfigPropertyBool>(Property);
        return float(int32(Obj->Value));
    } else if (Cast<UConfigPropertyString>(Property)) {
        UConfigPropertyString* Obj = Cast<UConfigPropertyString>(Property);
        if (Obj->Value.IsNumeric()) {
            return FCString::Atof(*Obj->Value);
        }
    } else if (Cast<UConfigPropertyInteger>(Property)) {
        UConfigPropertyInteger* Obj = Cast<UConfigPropertyInteger>(Property);
        return float(Obj->Value);
    }

    return 0.f;
}

int32 URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToInteger(UConfigProperty* Property) {
    if (Cast<UConfigPropertyInteger>(Property)) {
        UConfigPropertyInteger* Obj = Cast<UConfigPropertyInteger>(Property);
        return Obj->Value;
    } else if (Cast<UConfigPropertyFloat>(Property)) {
        UConfigPropertyFloat* Obj = Cast<UConfigPropertyFloat>(Property);
        return int32(Obj->Value);
    } else if (Cast<UConfigPropertyBool>(Property)) {
        UConfigPropertyBool* Obj = Cast<UConfigPropertyBool>(Property);
        return int32(Obj->Value);
    } else if (Cast<UConfigPropertyString>(Property)) {
        UConfigPropertyString* Obj = Cast<UConfigPropertyString>(Property);
        if (Obj->Value.IsNumeric()) {
            return int32(FCString::Atof(*Obj->Value));
        }
    }

    return 0;
}

bool URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToBool(UConfigProperty* Property) {
    if (Cast<UConfigPropertyBool>(Property)) {
        UConfigPropertyBool* Obj = Cast<UConfigPropertyBool>(Property);
        return int32(Obj->Value);
    } else if (Cast<UConfigPropertyInteger>(Property)) {
        UConfigPropertyInteger* Obj = Cast<UConfigPropertyInteger>(Property);
        if (Obj->Value < 2 && Obj->Value >= 0) return bool(Obj->Value);
        else return false;
    } else if (Cast<UConfigPropertyFloat>(Property)) {
        UConfigPropertyFloat* Obj = Cast<UConfigPropertyFloat>(Property);
        int32 Property = int32(Obj->Value);
        if (Property == 1 || Property == 0) return bool(Property);
        else return false;
    } else if (Cast<UConfigPropertyString>(Property)) {
        UConfigPropertyString* Obj = Cast<UConfigPropertyString>(Property);
        if (Obj->Value.IsNumeric()) {
            int32 Property = int32(FCString::Atof(*Obj->Value));
            if (Property == 1 || Property == 0) return bool(Property);
            else return false;
        }
    }

    return 0;
};

float URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyFloatToFloat(UConfigPropertyFloat* Property) {
    return Conv_ConfigPropertyToFloat(Property);
};

int32 URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyIntegerToInteger(UConfigPropertyInteger* Property) {
    return Conv_ConfigPropertyToInteger(Property);
};

bool URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyBoolToBool(UConfigPropertyBool* Property) {
    return Conv_ConfigPropertyToBool(Property);
};

FString URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyStringToString(UConfigPropertyString* Property) {
    return Conv_ConfigPropertyToString(Property);
};

UConfigProperty* URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertySectionToConfigProperty(UConfigPropertySection* Property, FString Key) {
    if (!Property) return nullptr;
    if (!Property->SectionProperties.Contains(Key)) return nullptr;
    UConfigProperty* Out = *Property->SectionProperties.Find(Key);
    return Out;
};

TArray<UConfigProperty*> URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyArrayToConfigPropertyArray(UConfigPropertyArray* Property) {
    if (!Property) return TArray<UConfigProperty*>();
    return Property->Values;
};

TArray<FString> URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyArrayToStringArray(UConfigPropertyArray* Property) {
    TArray<FString> Out;
    if (Property)
        for (auto* i : Property->Values) Out.Add(Conv_ConfigPropertyToString(i));
    return Out;
};

TArray<float> URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyArrayToFloatArray(UConfigPropertyArray* Property) {
    TArray<float> Out;
    if (Property)
        for (auto* i : Property->Values) Out.Add(Conv_ConfigPropertyToFloat(i));
    return Out;
};

TArray<int32> URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyArrayToIntegerArray(UConfigPropertyArray* Property) {
    TArray<int32> Out;
    if (Property)
        for (auto* i : Property->Values) Out.Add(Conv_ConfigPropertyToInteger(i));
    return Out;
}

TArray<bool> URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyArrayToBoolArray(UConfigPropertyArray* Property) {
    TArray<bool> Out;
    if (Property)
        for (auto* i : Property->Values) Out.Add(Conv_ConfigPropertyToBool(i));
    return Out;
};

UWidget* URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToWidget(UConfigProperty* Property, UObject* ParentWidget) {
    if (!Property) return nullptr;
    return Property->CreateEditorWidget(Cast<UUserWidget>(ParentWidget));
};

UUserWidget* URuntimeBlueprintFunctionLibrary::Conv_ConfigPropertyToUserWidget(UConfigProperty* Property, UObject* ParentWidget) {
    return Cast<UUserWidget>(Conv_ConfigPropertyToWidget(Property, ParentWidget));
};

UConfigProperty* URuntimeBlueprintFunctionLibrary::GetModConfigurationProperty(const FConfigId& ConfigId) {
    const UConfigManager* MySubsystem = GEngine->GetEngineSubsystem<UConfigManager>();
    const FRegisteredConfigurationData* ConfigurationData = MySubsystem->Configurations.Find(ConfigId);
    if (!ConfigurationData) {
        return nullptr;
    } else {
        return ConfigurationData->RootValue->GetWrappedValue();
    }
};

UConfigProperty* URuntimeBlueprintFunctionLibrary::GetModConfigurationPropertyByClass(TSubclassOf<UModConfiguration> ConfigClass) {
    if (!ConfigClass) return nullptr;
    UConfigManager* ConfigManager = GEngine->GetEngineSubsystem<UConfigManager>();
#if WITH_EDITOR
    // Assuming we have not registered in Editor World or similiar
    // But also considering this might work in the Editor once someone emulates the Startup Registration on PIE Play
    // we check only in Editor if Configurations are Empty.
    // Editor Widgets that are used while not in PIE dont have proper World Context.
    // For this Function to work regardless on Play State in Editor
    // This Function "Simulates" it being where it would be inGame
    // This return is for Comsetic and visual pre-display only
    // while also allowing modification of Default Values.
    if (ConfigManager->Configurations.Num() == 0) {
        return ConfigClass.GetDefaultObject()->RootSection;
    }
#endif
    for (auto i : ConfigManager->Configurations) {
        if (i.Value.ConfigurationClass == ConfigClass) {
            return i.Value.RootValue->GetWrappedValue();
        }
    }
    return nullptr;
};

UConfigProperty* URuntimeBlueprintFunctionLibrary::Conv_ModConfigurationToConfigProperty(TSubclassOf<UModConfiguration> ConfigClass) {
    return (GetModConfigurationPropertyByClass(ConfigClass));
}

UConfigProperty* URuntimeBlueprintFunctionLibrary::Conv_ConfigIdToConfigProperty(const FConfigId& ConfigId) {
    return GetModConfigurationProperty(ConfigId);
}

TSubclassOf<UModConfiguration> URuntimeBlueprintFunctionLibrary::Conv_ConfigIdToModConfigurationClass(const FConfigId& ConfigId) {
    const UConfigManager* MySubsystem = GEngine->GetEngineSubsystem<UConfigManager>();
    const FRegisteredConfigurationData* ConfigurationData = MySubsystem->Configurations.Find(ConfigId);
    if (!ConfigurationData) {
        return nullptr;
    } else {
        return ConfigurationData->ConfigurationClass;
    }
}

UUserWidget* URuntimeBlueprintFunctionLibrary::CreateSMLWidget(UUserWidget* OwningObject, TSubclassOf<UUserWidget> UserWidgetClass, FName WidgetName) {
    SCOPE_CYCLE_COUNTER(STAT_CreateWidget);

    if (OwningObject) {
        return Cast<UUserWidget>(UUserWidget::CreateWidgetInstance(*OwningObject, UserWidgetClass, WidgetName));
    }
    return nullptr;
}

void URuntimeBlueprintFunctionLibrary::SetSpinBoxFont(USpinBox* Box, FSlateFontInfo Font) {
    if (!Box) return;
    Box->Font = Font;
}

void URuntimeBlueprintFunctionLibrary::SetComboBoxFont(UComboBoxString* Box, FSlateFontInfo Font) {
    if (!Box) return;
    Box->Font = Font;
}
