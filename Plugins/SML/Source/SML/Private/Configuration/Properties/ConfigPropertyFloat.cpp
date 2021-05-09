#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"
#include "Configuration/CodeGeneration/ConfigVariableLibrary.h"
#include "Configuration/RawFileFormat/RawFormatValueNumber.h"
#include "Reflection/BlueprintReflectedObject.h"

UConfigPropertyFloat::UConfigPropertyFloat() {
    this->Value = 0.0f;
}

FString UConfigPropertyFloat::DescribeValue_Implementation() const {
    return FString::Printf(TEXT("[float %f]"), Value);
}

URawFormatValue* UConfigPropertyFloat::Serialize_Implementation(UObject* Outer) const {
    URawFormatValueNumber* NumberValue = NewObject<URawFormatValueNumber>(Outer);
    NumberValue->SetValueFloat(Value);
    return NumberValue;
}

void UConfigPropertyFloat::Deserialize_Implementation(const URawFormatValue* RawValue) {
    const URawFormatValueNumber* NumberValue = Cast<URawFormatValueNumber>(RawValue);
    if (NumberValue != NULL) {
        this->Value = NumberValue->Value;
    }
}

void UConfigPropertyFloat::FillConfigStruct_Implementation(const FReflectedObject& ReflectedObject, const FString& VariableName) const {
    ReflectedObject.SetFloatProperty(*VariableName, Value);
}

FConfigVariableDescriptor UConfigPropertyFloat::CreatePropertyDescriptor_Implementation
(
    UConfigGenerationContext* Context,
    const FString& OuterPath
) const {
    return UConfigVariableLibrary::MakeConfigVariablePrimitive(EConfigVariableType::ECVT_Float);
}
