#pragma once
#include "Configuration/RawFileFormat/RawFormatValue.h"
#include "RawFormatValueNumber.generated.h"

/** Describes raw number value */
UCLASS()
class SML_API URawFormatValueNumber : public URawFormatValue {
    GENERATED_BODY()
public:
    /** Actual value stored, as double precision floating point number */
    UPROPERTY()
    double Value;

    /** Retrieves number value as floating point */
    UFUNCTION(BlueprintPure)
    FORCEINLINE float AsFloat() const { return (float)Value; };

    /** Updates raw value to specified floating point value */
    UFUNCTION(BlueprintCallable)
    void SetValueFloat(float NewValue);

    /** Retrieves number value as 32-bit signed integer */
    UFUNCTION(BlueprintPure)
    FORCEINLINE int32 AsInt() const { return (int32)Value; }

    /** Updates raw value to specified 32-bit signed integer */
    UFUNCTION(BlueprintCallable)
    void SetValueInt(int32 NewValue);
};

FORCEINLINE void URawFormatValueNumber::SetValueFloat(float NewValue) {
    this->Value = NewValue;
}

FORCEINLINE void URawFormatValueNumber::SetValueInt(int32 NewValue) {
    this->Value = NewValue;
}
