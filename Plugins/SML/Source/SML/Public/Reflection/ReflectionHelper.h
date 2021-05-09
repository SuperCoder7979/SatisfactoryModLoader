#pragma once
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "ReflectionHelper.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct SML_API FDynamicStructInfo {
    GENERATED_BODY()
public:
    UPROPERTY()
    UScriptStruct* Struct;
    void* StructValue;
};

class SML_API FReflectionHelper {
public:
    /** Checks passed struct parameter and wraps it into the nice USTRUCT wrapper containing type and actual value */
    static FORCEINLINE FDynamicStructInfo CheckStructParameter(UObject* Context, FFrame& Stack) {
        Stack.MostRecentProperty = NULL;
        Stack.MostRecentPropertyAddress = NULL;
        Stack.StepCompiledIn<FStructProperty>(NULL);

        FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
        if (StructProperty == NULL) {
            const FBlueprintExceptionInfo ExceptionInfo(
                EBlueprintExceptionType::AccessViolation,
                INVTEXT("Tried to pass non-struct object to UFUNCTION. Only structs are supported")
            );
            FBlueprintCoreDelegates::ThrowScriptException(Context, Stack, ExceptionInfo);
            return FDynamicStructInfo{NULL, NULL};
        }

        if (Stack.MostRecentPropertyAddress == NULL) {
            const FBlueprintExceptionInfo ExceptionInfo(
                EBlueprintExceptionType::AccessViolation,
                INVTEXT("Tried to pass NULL struct object to UFUNCTION.")
            );
            FBlueprintCoreDelegates::ThrowScriptException(Context, Stack, ExceptionInfo);
            return FDynamicStructInfo{NULL, NULL};
        }
        void* StructValue = Stack.MostRecentPropertyAddress;
        return FDynamicStructInfo{StructProperty->Struct, StructValue};
    }

    template<typename T>
    static T* FindPropertyChecked(UStruct* Class, const TCHAR* PropertyName) {
        T* Property = Cast<T>(Class->FindPropertyByName(PropertyName));
        checkf(Property, TEXT("Property with given name not found in class: %s"), PropertyName);
        return Property;
    }

    template<typename T>
    static T* FindPropertyByShortNameChecked(UStruct* Class, const TCHAR* PropertyName) {
        for (FProperty* Property = Class->PropertyLink; Property; Property = Property->PropertyLinkNext) {
            if (Property->GetName().StartsWith(PropertyName)) {
                if (T* CastedProperty = Cast<T>(Property)) {
                    return CastedProperty;
                }
            }
        }
        checkf(false, TEXT("Property with given name not found in class: %s"), PropertyName);
        return nullptr;
    }

    template<typename T>
    static typename T::TCppType GetPropertyValue(const UObject* Object, const TCHAR* PropertyName, int32 ArrayIndex = 0) {
        T* Property = Cast<T>(Object->GetClass()->FindPropertyByName(PropertyName));
        checkf(Property, TEXT("Property not found in class %s: %s"), *Object->GetClass()->GetPathName(), PropertyName);
        return Property->GetPropertyValue_InContainer(Object, ArrayIndex);
    }

    template<typename T>
    static T* GetObjectPropertyValue(const UObject* Object, const TCHAR* PropertyName) {
        UObject* FieldValue = GetPropertyValue<FObjectProperty>(Object, PropertyName);
        T* CastedValue = Cast<T>(FieldValue);
        checkf(CastedValue, TEXT("Cannot cast class property %s:%s value to type %s"), *Object->GetClass()->GetPathName(), PropertyName, *T::StaticClass()->GetName());
        return CastedValue;
    }

    static void SetStructPropertyValue(UObject* Object, const TCHAR* PropertyName, const void* ValuePointer, int32 ArrayIndex = 0) {
        FStructProperty* Property = CastField<FStructProperty>(Object->GetClass()->FindPropertyByName(PropertyName));
        checkf(Property, TEXT("Property not found in class %s: %s"), *Object->GetClass()->GetPathName(), PropertyName);
        void* DestAddress = Property->ContainerPtrToValuePtr<void>(Object, ArrayIndex);
        Property->CopyValuesInternal(DestAddress, ValuePointer, 1);
    }

    template<typename T>
    static void SetPropertyValue(UObject* Object, const TCHAR* PropertyName, const typename T::TCppType& Value, int32 ArrayIndex = 0) {
        T* Property = CastField<T>(Object->GetClass()->FindPropertyByName(PropertyName));
        checkf(Property, TEXT("Property not found in class %s: %s"), *Object->GetClass()->GetPathName(), PropertyName);
        Property->SetPropertyValue_InContainer(Object, Value, ArrayIndex);
    }

    static FProperty* FindParameterByIndex(UFunction* Function, int32 Index) {
        int32 CurrentIndex = 0;
        for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It) {
            if (CurrentIndex++ == Index) return *It;
        }
        return NULL;
    }

    template<typename T>
    static UFunction* CallScriptFunction(UObject* Object, const TCHAR* FunctionName, T* ParamStruct) {
        UFunction* Function = Object->FindFunction(FunctionName);
        checkf(Function, TEXT("Function not found: %s"), FunctionName);
        checkf(
            Function->ParmsSize == static_cast<uint16>(sizeof(T)) || (Function->ParmsSize == 0 && ParamStruct == NULL),
            TEXT("Function parameter layout doesn't match provided parameter struct: Expected %d bytes, got %u"),
            Function->ParmsSize,
            sizeof(T)
        );
        Object->ProcessEvent(Function, ParamStruct);
        return Function;
    }
};
