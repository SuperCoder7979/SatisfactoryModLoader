#include "Configuration/ConfigProperty.h"
#include "Configuration/ConfigValueDirtyHandlerInterface.h"
#include "Configuration/CodeGeneration/ConfigGenerationContext.h"
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"

FString UConfigProperty::DescribeValue_Implementation() const {
    return FString::Printf(TEXT("[unknown value %s]"), *GetClass()->GetPathName());
}

URawFormatValue* UConfigProperty::Serialize_Implementation(UObject* Outer) const {
    checkf(false, TEXT("Serialize not implemented"));
    return NULL;
}

void UConfigProperty::Deserialize_Implementation(const URawFormatValue* Value) {
    checkf(false, TEXT("Deserialize not implemented"));
}

void UConfigProperty::MarkDirty() {
    // Let closest Outer object implementing IConfigValueDirtyHandlerInterface handle MarkDirty call
    for (UObject* NextOuter = GetOuter(); NextOuter != NULL; NextOuter = NextOuter->GetOuter()) {
        if (NextOuter->Implements<UConfigValueDirtyHandlerInterface>()) {
            IConfigValueDirtyHandlerInterface::Execute_MarkDirty(NextOuter);
            break;
        }
    }
}

void UConfigProperty::FillConfigStruct_Implementation
(
    const FReflectedObject& ReflectedObject,
    const FString& VariableName
) const {
    checkf(false, TEXT("FillConfigStruct not implemented"));
}

FConfigVariableDescriptor UConfigProperty::CreatePropertyDescriptor_Implementation(UConfigGenerationContext* Context, const FString& OuterPath) const {
    checkf(false, TEXT("CreatePropertyDescriptor not implemented"));
    return FConfigVariableDescriptor{};
}

UUserWidget* UConfigProperty::CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const {
    return NULL;
}
