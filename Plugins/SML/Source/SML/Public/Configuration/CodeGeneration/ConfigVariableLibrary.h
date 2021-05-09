#pragma once
#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"
#include "Reflection/ReflectionHelper.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConfigVariableLibrary.generated.h"

UCLASS()
class SML_API UConfigVariableLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:
    /** Returns type of the passed config variable descriptor */
    UFUNCTION(BlueprintPure, Category = "ConfigVariables", meta = (BlueprintThreadSafe))
    static FORCEINLINE EConfigVariableType GetVariableType(const FConfigVariableDescriptor& Descriptor) { return Descriptor.GetVariableType(); }

    /** Returns full path to the custom structure associated with this variable */
    UFUNCTION(BlueprintPure, Category = "ConfigVariables", meta = (BlueprintThreadSafe))
    static FORCEINLINE FString GetCustomStructType(const FConfigVariableDescriptor& Descriptor) { return Descriptor.GetCustomStructType()->GetPathName(); }

    /** Returns full path to the custom structure associated with this variable */
    UFUNCTION(BlueprintPure, Category = "ConfigVariables", meta = (BlueprintThreadSafe))
    static FORCEINLINE class UConfigGeneratedStruct* GetConfigGeneratedStruct(const FConfigVariableDescriptor& Descriptor) { return Descriptor.GetConfigGeneratedStruct(); }

    /** Returns descriptor for array element type of this variable */
    UFUNCTION(BlueprintPure, Category = "ConfigVariables", meta = (BlueprintThreadSafe))
    static FORCEINLINE FConfigVariableDescriptor GetArrayElementType(const FConfigVariableDescriptor& Descriptor) { return Descriptor.GetArrayElementDescriptor(); }

    /** Returns descriptor for map key type of this variable */
    UFUNCTION(BlueprintPure, Category = "ConfigVariables", meta = (BlueprintThreadSafe))
    static FORCEINLINE FConfigVariableDescriptor GetMapKeyType(const FConfigVariableDescriptor& Descriptor) { return Descriptor.GetMapKeyDescriptor(); }

    /** Returns descriptor for map value type of this variable */
    UFUNCTION(BlueprintPure, Category = "ConfigVariables", meta = (BlueprintThreadSafe))
    static FORCEINLINE FConfigVariableDescriptor GetMapValueType(const FConfigVariableDescriptor& Descriptor) { return Descriptor.GetMapValueDescriptor(); }

    /** Returns true if the values are equal (A == B) */
    UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (ConfigVariableDescriptor)", CompactNodeTitle="==", BlueprintThreadSafe), Category = "ConfigVariables")
    static FORCEINLINE bool EqualEqual_FConfigVariableDescriptor(const FConfigVariableDescriptor& A, const FConfigVariableDescriptor& B) { return A == B; }

    /** Returns true if the values are not equal (A != B) */
    UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (ConfigVariableDescriptor)", CompactNodeTitle="!=", BlueprintThreadSafe), Category = "ConfigVariables")
    static FORCEINLINE bool NotEqual_FConfigVariableDescriptor(const FConfigVariableDescriptor& A, const FConfigVariableDescriptor& B) { return A != B; }

    /** Creates new instance of primitive (number, string) variable */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", meta=(DisplayName = "Make Config Variable (Primitive)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariablePrimitive(EConfigVariableType VariableType);

    /** Creates new instance of custom structure config variable */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", CustomThunk, meta=(CustomStructureParam = "Struct", DisplayName = "Make Config Variable (struct)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariableStruct(const FDynamicStructInfo& Struct);

    /** Creates new instance of object variable with base class type provided */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", meta=(DisplayName = "Make Config Variable (Object)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariableObject(UClass* BaseObjectClass);

    /** Creates new instance of class variable with base parent class type specified */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", meta=(DisplayName = "Make Config Variable (Class)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariableClass(UClass* BaseClassType);

    /** Creates new instance of nested config generated struct variable */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", meta=(DisplayName = "Make Config Variable (Config Struct)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariableGeneratedStruct(class UConfigGeneratedStruct* Struct);

    /** Creates new instance of array config variable with specified inner element type */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", meta=(DisplayName = "Make Config Variable (Array)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariableArray(const FConfigVariableDescriptor& ElementType);

    /** Creates new instance of map config variable with specified key and value types */
    UFUNCTION(BlueprintPure, Category="ConfigVariables", meta=(DisplayName = "Make Config Variable (Map)", Keywords="construct build", NativeMakeFunc))
    static FConfigVariableDescriptor MakeConfigVariableMap(const FConfigVariableDescriptor& KeyType, const FConfigVariableDescriptor& ValueType);

    DECLARE_FUNCTION(execMakeConfigVariableStruct) {
        const FDynamicStructInfo StructInfo = FReflectionHelper::CheckStructParameter(Context, Stack);
        P_FINISH;
        if (StructInfo.Struct != NULL) {
            MakeConfigVariableStruct(StructInfo);
        }
    }
};
