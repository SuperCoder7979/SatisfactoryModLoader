#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"
#include "Reflection/BlueprintReflectedObject.h"
#include "ConfigProperty.generated.h"

class URawFormatValue;
class UUserWidget;

/**
 * Describes single value inside the configuration
 * Value doesn't actually hold any mutable data, instead, it manages
 * ConfigurationState and provides method for working with it
 * It provides editing functionality for state, and default value to start from
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SML_API UConfigProperty : public UObject {
    GENERATED_BODY()
public:
    /** Display name of this property as it is visible to the user */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration Property")
    FString DisplayName;

    /** Tooltip visible to user hovering over this property */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration Property")
    FString Tooltip;

    /** Whenever this value is only editable from main menu and disabled for editing in pause menu */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration Property")
    uint8 bRequiresWorldReload: 1;

    /** Whenever this value should be hidden in Widgets ( No User Input )  */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration Property")
    uint8 bHidden : 1;

    /** Describes value of this property for debugging purposes */
    UFUNCTION(BlueprintPure, BlueprintNativeEvent)
    FString DescribeValue() const;

    /** Serializes value of this property into raw file format. Please use specified Outer for creating raw value object */
    UFUNCTION(BlueprintPure, BlueprintNativeEvent, meta = (DefaultToSelf = "Outer"))
    URawFormatValue* Serialize(UObject* Outer) const;

    /** Deserializes passed raw file format value into this property state */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    void Deserialize(const URawFormatValue* Value);

    /** Marks this property directly, forcing file system synchronization to happen afterwards */
    UFUNCTION(BlueprintCallable)
    virtual void MarkDirty();

    /** Creates widget instance for editing this configuration property's value. Can return NULL if property doesn't support direct UI editing */
    UFUNCTION(BlueprintPure, BlueprintNativeEvent, meta = (DefaultToSelf = "ParentWidget"))
    UUserWidget* CreateEditorWidget(class UUserWidget* ParentWidget) const;

    /** Creates config property descriptor for code generation */
    UFUNCTION(BlueprintPure, BlueprintNativeEvent)
    FConfigVariableDescriptor CreatePropertyDescriptor(class UConfigGenerationContext* Context, const FString& OuterPath) const;

    /** Fills variable of provided object with the value carried by this property */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    void FillConfigStruct(const FReflectedObject& ReflectedObject, const FString& VariableName) const;
};
