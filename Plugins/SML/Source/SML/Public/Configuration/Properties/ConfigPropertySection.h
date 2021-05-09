#pragma once
#include "Configuration/ConfigProperty.h"
#include "ConfigPropertySection.generated.h"

/** Describes a single configuration section with nested properties */
UCLASS()
class SML_API UConfigPropertySection : public UConfigProperty {
    GENERATED_BODY()
public:
    /**
     * All properties contained in this section
     * Consider using alphanumerical characters only in property names
     */
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Configuration Property")
    TMap<FString, UConfigProperty*> SectionProperties;

    /** Fills reflected object with the values of this section */
    void FillConfigStructSelf(const FReflectedObject& ReflectedObject) const;

    // Begin UObject
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
    // End UObject

    // Begin UConfigProperty
    virtual FString DescribeValue_Implementation() const override;
    virtual URawFormatValue* Serialize_Implementation(UObject* Outer) const override;
    virtual void Deserialize_Implementation(const URawFormatValue* Value) override;
    FConfigVariableDescriptor CreatePropertyDescriptor_Implementation(UConfigGenerationContext* Context, const FString& OuterPath) const override;
    void FillConfigStruct_Implementation(const FReflectedObject& ReflectedObject, const FString& VariableName) const override;
    // End UConfigProperty
};
