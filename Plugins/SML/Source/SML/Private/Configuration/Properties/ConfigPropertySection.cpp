#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/CodeGeneration/ConfigGenerationContext.h"
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"
#include "Configuration/CodeGeneration/ConfigVariableLibrary.h"
#include "Configuration/RawFileFormat/RawFormatValueObject.h"
#include "Reflection/BlueprintReflectedObject.h"
#define LOCTEXT_NAMESPACE "SML"

FString UConfigPropertySection::DescribeValue_Implementation() const {
    const FString ElementsString = FString::JoinBy(
        SectionProperties,
        TEXT(", "),
        [this](const TPair<FString, UConfigProperty*>& Property) {
            return FString::Printf(TEXT("%s = %s"), *Property.Key, *Property.Value->DescribeValue());
        }
    );
    return FString::Printf(TEXT("[section %s]"), *ElementsString);
}

URawFormatValue* UConfigPropertySection::Serialize_Implementation(UObject* Outer) const {
    URawFormatValueObject* ObjectValue = NewObject<URawFormatValueObject>(Outer);
    for (const TPair<FString, UConfigProperty*>& Property : SectionProperties) {
        if (Property.Value != NULL) {
            URawFormatValue* RawChildValue = Property.Value->Serialize(ObjectValue);
            if (RawChildValue != NULL) {
                ObjectValue->AddValue(Property.Key, RawChildValue);
            }
        }
    }
    return ObjectValue;
}

void UConfigPropertySection::Deserialize_Implementation(const URawFormatValue* Value) {
    const URawFormatValueObject* ObjectValue = Cast<URawFormatValueObject>(Value);
    if (ObjectValue != NULL) {
        for (const TPair<FString, UConfigProperty*>& Property : SectionProperties) {
            const URawFormatValue* RawChildValue = ObjectValue->GetValue(Property.Key);
            if (Property.Value != NULL && RawChildValue != NULL) {
                Property.Value->Deserialize(RawChildValue);
            }
        }
    }
}

void UConfigPropertySection::FillConfigStructSelf(const FReflectedObject& ReflectedObject) const {
    for (const TPair<FString, UConfigProperty*>& Property : SectionProperties) {
        if (Property.Value != NULL) {
            Property.Value->FillConfigStruct(ReflectedObject, Property.Key);
        }
    }
}

bool IsValidCppIdentifier(const FString& Identifier) {
    for (int32 i = 0; i < Identifier.Len(); i++) {
        if (!IsValidCPPIdentifierChar(Identifier[i])) {
            return false;
        }
    }
    return Identifier.Len() > 0;
}

bool IsValidConfigIdentifier(const FString& Identifier) {
    return IsValidCppIdentifier(Identifier) && FName(*Identifier).IsValidXName(INVALID_OBJECTNAME_CHARACTERS);
}

#if WITH_EDITOR
EDataValidationResult UConfigPropertySection::IsDataValid(TArray<FText>& ValidationErrors) {
    EDataValidationResult ValidationResult = EDataValidationResult::Valid;
    for (const TPair<FString, UConfigProperty*>& Pair : SectionProperties) {
        if (!IsValid(Pair.Value)) {
            ValidationErrors.Add(FText::Format(LOCTEXT("ConfigSectionValueNull",
                "Value '{0}' inside of the Config Section '{1}' is NULL."),
                FText::FromString(Pair.Key), FText::FromString(GetPathName())));
            ValidationResult = EDataValidationResult::Invalid;
        }
        if (!IsValidConfigIdentifier(Pair.Key)) {
            ValidationErrors.Add(FText::Format(LOCTEXT("ConfigSectionInvalidKey",
                "Section Key '{0}' inside of the Config Section '{1}' is Invalid. "
                "Make sure it contains only alphanumerical characters and does not contain spaces."),
                FText::FromString(Pair.Key), FText::FromString(GetPathName())));
            ValidationResult = EDataValidationResult::Invalid;
        }
        if (IsValid(Pair.Value)) {
            const EDataValidationResult ChildValidationResult = Pair.Value->IsDataValid(ValidationErrors);
            if (ChildValidationResult == EDataValidationResult::Invalid) {
                ValidationResult = EDataValidationResult::Invalid;
            }
        }
    }
    return ValidationResult;
}
#endif

void UConfigPropertySection::FillConfigStruct_Implementation(const FReflectedObject& ReflectedObject, const FString& VariableName) const {
    const FReflectedObject ChildObject = VariableName.IsEmpty() ? ReflectedObject : ReflectedObject.GetStructProperty(*VariableName);
    FillConfigStructSelf(ChildObject);
    ReflectedObject.SetStructProperty(*VariableName, ChildObject);
}

FConfigVariableDescriptor UConfigPropertySection::CreatePropertyDescriptor_Implementation
(
    UConfigGenerationContext* Context,
    const FString& OuterPath
) const {
    UConfigGeneratedStruct* GeneratedStruct = Context->CreateNewConfigStruct(OuterPath);
    for (const TPair<FString, UConfigProperty*>& Property : SectionProperties) {
        const FString InnerPath = FString::Printf(TEXT("%s_%s"), *OuterPath, *Property.Key);
        const FConfigVariableDescriptor Descriptor = Property.Value->CreatePropertyDescriptor(Context, InnerPath);
        GeneratedStruct->AddConfigVariable(Descriptor, Property.Key);
    }
    return UConfigVariableLibrary::MakeConfigVariableGeneratedStruct(GeneratedStruct);
}
