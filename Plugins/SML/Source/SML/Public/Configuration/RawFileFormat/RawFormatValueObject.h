#pragma once
#include "Templates/SubclassOf.h"
#include "Configuration/RawFileFormat/RawFormatValueArray.h"
#include "RawFormatValueObject.generated.h"

/** Describes raw map (also known as object) value */
UCLASS()
class SML_API URawFormatValueObject : public URawFormatValue {
    GENERATED_BODY()
public:
    /** Actual of raw map object */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, URawFormatValue*> Values;

    UFUNCTION(BlueprintPure)
    FORCEINLINE URawFormatValue* GetValue(const FString& Key) const {
        URawFormatValue* const* Value = Values.Find(Key);
        return Value ? *Value : NULL;
    }

    /** Retrieves value at the given key as array */
    UFUNCTION(BlueprintPure)
    FORCEINLINE URawFormatValueArray* GetArray(const FString& Key) const {
        return Cast<URawFormatValueArray>(GetValue(Key));
    }

    UFUNCTION(BlueprintCallable)
    FORCEINLINE URawFormatValueArray* SetArray(const FString& Key) {
        return AddNewValue<URawFormatValueArray>(Key);
    }

    /** Retrieves value at the given key as object */
    UFUNCTION(BlueprintPure)
    FORCEINLINE URawFormatValueObject* GetObject(const FString& Key) const {
        return Cast<URawFormatValueObject>(GetValue(Key));
    }

    UFUNCTION(BlueprintCallable)
    FORCEINLINE URawFormatValueObject* SetObject(const FString& Key) {
        return AddNewValue<URawFormatValueObject>(Key);
    }

    /** Retrieves value at the given key as string, falling back to empty string if it's not a string */
    UFUNCTION(BlueprintPure)
    FORCEINLINE FString GetString(const FString& Key) const {
        URawFormatValueString* StringValue = Cast<URawFormatValueString>(GetValue(Key));
        return StringValue ? StringValue->Value : TEXT("");
    }

    UFUNCTION(BlueprintCallable)
    void SetString(const FString& Key, const FString& Value);

    /** Retrieves value at the given key as integer, falling back to zero if it's not a number */
    UFUNCTION(BlueprintPure)
    FORCEINLINE int32 GetInteger(const FString& Key) const {
        URawFormatValueNumber* NumberValue = Cast<URawFormatValueNumber>(GetValue(Key));
        return NumberValue ? NumberValue->AsInt() : 0;
    }

    UFUNCTION(BlueprintCallable)
    void SetInteger(const FString& Key, const int32 Value);

    /** Retrieves value at the given key as floating point number, falling back to zero if it's not a number */
    UFUNCTION(BlueprintPure)
    FORCEINLINE float GetFloat(const FString& Key) const {
        URawFormatValueNumber* NumberValue = Cast<URawFormatValueNumber>(GetValue(Key));
        return NumberValue ? NumberValue->AsFloat() : 0.0f;
    }

    UFUNCTION(BlueprintCallable)
    void SetFloat(const FString& Key, const float Value);

    /** Adds value into the object */
    UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "ValueClass"))
    void AddValue(const FString& Key, URawFormatValue* ValueClass);

    /** Adds value into the map and returns pointer to it. Replaces value with duplicate key */
    UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "ValueClass"))
    FORCEINLINE URawFormatValue* AddNewValue(const FString& Key, TSubclassOf<URawFormatValue> ValueClass) {
        URawFormatValue* NewValue = NewObject<URawFormatValue>(this, ValueClass);
        AddValue(Key, NewValue);
        return NewValue;
    }

    /** Template version of AddValue for C++ usage */
    template<typename T>
    FORCEINLINE T* AddNewValue(const FString& Key) {
        return Cast<T>(AddNewValue(Key, T::StaticClass()));
    }
};

FORCEINLINE void URawFormatValueObject::SetString(const FString& Key, const FString& Value) {
    AddNewValue<URawFormatValueString>(Key)->Value = Value;
}

FORCEINLINE void URawFormatValueObject::SetInteger(const FString& Key, const int32 Value) {
    AddNewValue<URawFormatValueNumber>(Key)->SetValueInt(Value);
}

FORCEINLINE void URawFormatValueObject::SetFloat(const FString& Key, const float Value) {
    AddNewValue<URawFormatValueNumber>(Key)->SetValueFloat(Value);
}

FORCEINLINE void URawFormatValueObject::AddValue(const FString& Key, URawFormatValue* ValueClass) {
    // Sanity check to ensure hierarchical view inside this raw value
    checkf(ValueClass && ValueClass->GetOuter() == this, TEXT("Cannot add URawFormatValue residuing inside another outer object (should be URawFormatValueArray)"));
    Values.Add(Key, ValueClass);
};
