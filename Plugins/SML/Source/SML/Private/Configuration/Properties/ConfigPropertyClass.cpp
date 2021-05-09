#include "Configuration/Properties/ConfigPropertyClass.h"
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"
#include "Configuration/CodeGeneration/ConfigVariableLibrary.h"
#include "Configuration/RawFileFormat/RawFormatValueString.h"
#include "Reflection/BlueprintReflectedObject.h"
#define LOCTEXT_NAMESPACE "SML"

UConfigPropertyClass::UConfigPropertyClass() {
    this->Value = NULL;
    BaseClass = UObject::StaticClass();
    bLimitBaseClass = false;
}

bool UConfigPropertyClass::IsValidValueClass(UClass* Class) const {
    return Class ? (bLimitBaseClass && BaseClass ? Class->IsChildOf(BaseClass) : true) : bAllowNullValue;
}

void UConfigPropertyClass::SetClassValue(UClass* NewValue) {
    if (IsValidValueClass(NewValue)) {
        this->Value = NewValue;
    }
}

#if WITH_EDITOR
EDataValidationResult UConfigPropertyClass::IsDataValid(TArray<FText>& ValidationErrors) {
    if (!IsValidValueClass(Value)) {
        ValidationErrors.Add(FText::Format(LOCTEXT("InvalidDefaultClassValue", "Class property '{0}' default value '{1}' is not valid "
            "with provided BaseClass/bAllowNullValue property settings"),
            FText::FromString(GetPathName()), FText::FromString(Value->GetPathName())));
        return EDataValidationResult::Invalid;
    }
    return EDataValidationResult::Valid;
}
#endif

FString UConfigPropertyClass::DescribeValue_Implementation() const {
    return FString::Printf(TEXT("[class %s]"), *Value->GetPathName());
}

URawFormatValue* UConfigPropertyClass::Serialize_Implementation(UObject* Outer) const {
    URawFormatValueString* StringValue = NewObject<URawFormatValueString>(Outer);
    StringValue->Value = Value->GetPathName();
    return StringValue;
}

void UConfigPropertyClass::Deserialize_Implementation(const URawFormatValue* RawValue) {
    const URawFormatValueString* StringValue = Cast<URawFormatValueString>(RawValue);
    if (StringValue != NULL) {
        if (StringValue->Value != TEXT("None")) {
            // String indicates full class path name, so use LoadObject<UClass> to actually load it
            // SetClassValue will take care of type checking provided class object
            UClass* LoadedClassObject = LoadObject<UClass>(NULL, *StringValue->Value);
            if (LoadedClassObject != NULL) {
                SetClassValue(LoadedClassObject);
            }
        } else {
            // String is equal to None, and None is a special value indicating NULL class
            // If we don't allow NULL value, IsClassValueValid will return false, and value will remain default
            SetClassValue(NULL);
        }
    }
}

void UConfigPropertyClass::FillConfigStruct_Implementation(const FReflectedObject& ReflectedObject, const FString& VariableName) const {
    ReflectedObject.SetObjectProperty(*VariableName, Value);
}

FConfigVariableDescriptor UConfigPropertyClass::CreatePropertyDescriptor_Implementation
(
    UConfigGenerationContext* Context,
    const FString& OuterPath
) const {
    return UConfigVariableLibrary::MakeConfigVariableClass(BaseClass);
}
