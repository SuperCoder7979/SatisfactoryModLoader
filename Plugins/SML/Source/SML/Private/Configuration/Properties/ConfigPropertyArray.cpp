#include "Configuration/Properties/ConfigPropertyArray.h"
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"
#include "Configuration/CodeGeneration/ConfigVariableLibrary.h"
#include "Configuration/RawFileFormat/RawFormatValueArray.h"
#include "Reflection/BlueprintReflectedObject.h"
#define LOCTEXT_NAMESPACE "SML"

UConfigProperty* UConfigPropertyArray::AddNewElement() {
    checkf(DefaultValue, TEXT("Cannot add new element without default value defined"));
    UConfigProperty* NewValueProperty = NewObject<UConfigProperty>(this, DefaultValue->GetClass(), NAME_None, RF_NoFlags, DefaultValue);
    Values.Add(NewValueProperty);
    return NewValueProperty;
}

void UConfigPropertyArray::RemoveElementAtIndex(int32 Index) {
    if (Index >= 0 && Index < Values.Num()) {
        Values.RemoveAt(Index);
    }
}

void UConfigPropertyArray::Clear() {
    Values.Empty();
}

#if WITH_EDITOR
bool UConfigPropertyArray::CanEditChange(const FProperty* InProperty) const {
    const bool bCanSuperChange = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UConfigPropertyArray, Values)) {
        return bCanSuperChange && IsValid(DefaultValue);
    }
    return bCanSuperChange;
}

EDataValidationResult UConfigPropertyArray::IsDataValid(TArray<FText>& ValidationErrors) {
 /*
    if (DefaultValue == NULL) {
        ValidationErrors.Add(FText::Format(LOCTEXT("ArrayDefaultValueConfigNotSet",
            "Default Value for Array Property '{0}' is not set."), FText::FromString(GetPathName())));
        return EDataValidationResult::Invalid;
    }
*/
    EDataValidationResult ValidationResult = EDataValidationResult::Valid;
    for (const UConfigProperty* ConfigProperty : Values) {
        if (ConfigProperty == NULL ||
            ConfigProperty->GetClass() != DefaultValue->GetClass()) {
            ValidationErrors.Add(FText::Format(LOCTEXT("ArrayValueInvalid",
                "Array Property '{0}' Value '{1}' is invalid, or does not have the same class as Default Value. "
                "Default Value Class is '{0}', Value Class is '{1}'."),
                FText::FromString(GetPathName()), FText::FromString(ConfigProperty->GetPathName()),
                FText::FromString(DefaultValue->GetClass()->GetName()),
                FText::FromString(ConfigProperty ? ConfigProperty->GetClass()->GetName() : TEXT("None"))));
            ValidationResult = EDataValidationResult::Invalid;
        }
    }
    return ValidationResult;
}
#endif

FString UConfigPropertyArray::DescribeValue_Implementation() const {
    const FString ElementsString = FString::JoinBy(
        Values,
        TEXT(", "),
        [](const UConfigProperty* Value) { return Value->DescribeValue(); }
    );
    return FString::Printf(TEXT("[array: %s]"), *ElementsString);
}

URawFormatValue* UConfigPropertyArray::Serialize_Implementation(UObject* Outer) const {
    URawFormatValueArray* SerializedArray = NewObject<URawFormatValueArray>(Outer);
    for (const UConfigProperty* Value : Values) {
        // Specify serialization array as outer for allocated value objects
        SerializedArray->AddValue(Value->Serialize(SerializedArray));
    }
    return SerializedArray;
}

void UConfigPropertyArray::Deserialize_Implementation(const URawFormatValue* Value) {
    const URawFormatValueArray* SerializedArray = Cast<URawFormatValueArray>(Value);
    if (SerializedArray != NULL) {
        // Empty array but reserve enough Slack space to keep all elements we are going to add
        Values.Empty(SerializedArray->Num());
        // Just iterate raw format array and deserialize each of it's items
        for (URawFormatValue* RawFormatValue : SerializedArray->GetUnderlyingArrayRef()) {
            UConfigProperty* AllocatedValue = AddNewElement();
            AllocatedValue->Deserialize(RawFormatValue);
        }
    }
}

void UConfigPropertyArray::FillConfigStruct_Implementation(const FReflectedObject& ReflectedObject, const FString& VariableName) const {
    const FReflectedObject ArrayObject = ReflectedObject.GetArrayProperty(*VariableName);
    ArrayObject.ClearArray();
    for (const UConfigProperty* Property : Values) {
        const int32 NewElementIndex = ArrayObject.AddNewArrayElement();
        check(NewElementIndex >= 0);
        Property->FillConfigStruct(ArrayObject, FString::FromInt(NewElementIndex));
    }
}

FConfigVariableDescriptor UConfigPropertyArray::CreatePropertyDescriptor_Implementation
(
    UConfigGenerationContext* Context,
    const FString& OuterPath
) const {
    return UConfigVariableLibrary::MakeConfigVariableArray(DefaultValue->CreatePropertyDescriptor(Context, OuterPath));
}
