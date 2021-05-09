#pragma once
#include "CoreMinimal.h"
#include "BlueprintReflectedObject.generated.h"

struct FReflectedPropertyInfo;

class SML_API FReflectedObjectState {
public:
    virtual ~FReflectedObjectState() = default;

    virtual TArray<FReflectedPropertyInfo> GetAllProperties() const;
    virtual FProperty* FindPropertyByName(FName PropertyName) const;
    virtual void* GetPropertyValue(FName PropertyName);

    virtual int32 GetArrayNum() const;
    virtual int32 AddNewArrayElement() const;
    virtual void RemoveArrayElements(int32 Index, int32 Count) const;

    virtual void CopyStructValue(UScriptStruct* StructType, void* StructValue) const;
    virtual UObject* GetObjectPointer() const;
    virtual bool ShouldStripPropertyNames() const;
    virtual UStruct* GetStructObject() const;
    virtual void* GetObjectData();
    virtual void AddReferencedObjects(FReferenceCollector& ReferenceCollector) = 0;
};

/** Describes type of the reflected property */
UENUM(BlueprintType)
enum class EReflectedPropertyType : uint8 {
    ERPT_Invalid UMETA(DisplayName = "Invalid"),
    ERPT_Array UMETA(DisplayName = "Array"),
    ERPT_Byte UMETA(DisplayName = "Byte"),
    ERPT_Int32 UMETA(DisplayName = "Int"),
    ERPT_Int64 UMETA(DisplayName = "Int64"),
    ERPT_Float UMETA(DisplayName = "Float"),
    ERPT_String UMETA(DisplayName = "String"),
    ERPT_Bool UMETA(DisplayName = "Boolean"),
    ERPT_Name UMETA(DisplayName = "Name"),
    ERPT_Text UMETA(DisplayName = "Text"),
    ERPT_Struct UMETA(DisplayName = "Struct"),
    ERPT_Object UMETA(DisplayName = "Object"),
    ERPT_Enum UMETA(DisplayName = "Enum")
    // TODO interface support?
};

/** Describes a single reflected property accessible to blueprints */
USTRUCT(BlueprintType)
struct SML_API FReflectedPropertyInfo {
    GENERATED_BODY()
public:
    /** Name of the reflected property */
    UPROPERTY(BlueprintReadOnly)
    FName PropertyName;

    /** Type of the reflected property */
    UPROPERTY(BlueprintReadOnly)
    EReflectedPropertyType PropertyType;
};

/** Describes reflected enumeration value */
USTRUCT(BlueprintType)
struct SML_API FReflectedEnumValue {
    GENERATED_BODY()
public:
    FReflectedEnumValue();
    FReflectedEnumValue(UEnum* EnumType, int64 EnumValue);

    int64 GetCurrentValue() const;
    void SetCurrentValue(int64 NewValue);
    UEnum* GetEnumerationType() const;
private:
    UPROPERTY()
    UEnum* EnumerationType;
    /** Raw enumeration value represented as integer */
    UPROPERTY()
    int64 RawEnumValue;
};

/** Describes a reflected object such as script struct or UObject with a set of associated properties */
USTRUCT(BlueprintType)
struct SML_API FReflectedObject {
    GENERATED_BODY()
public:
    /** Setups state of this object from passed UObject pointer */
    void SetupFromUObject(UObject* Object);

    /** Setups state of this object from passed struct type and data */
    void SetupFromStruct(UScriptStruct* StructType, const void* StructData = NULL);

    /** Setups this reflected object as an array propert of passed object */
    void SetupFromArray(const FReflectedObject& Object, const FName PropertyName);

    /** Returns pointer to the wrapped object, if it's an object wrapper */
    UObject* GetWrappedObject() const;

    /** Copies wrapped struct value into the output struct */
    void CopyWrappedStruct(UScriptStruct* StructType, void* StructValue) const;

    template<typename T>
    FORCEINLINE void CopyWrappedStruct(T& OutStruct) const {
        CopyWrappedStruct(T::StaticStruct(), &OutStruct);
    }

    /** Returns a list of all reflected properties exposed by this object */
    TArray<FReflectedPropertyInfo> GetReflectedProperties() const;

    // Begin UProperty accessors
    uint8 GetByteProperty(FName PropertyName) const;
    void SetByteProperty(FName PropertyName, uint8 Value) const;

    int32 GetIntProperty(FName PropertyName) const;
    void SetIntProperty(FName PropertyName, int32 Value) const;

    int64 GetInt64Property(FName PropertyName) const;
    void SetInt64Property(FName PropertyName, int64 Value) const;

    float GetFloatProperty(FName PropertyName) const;
    void SetFloatProperty(FName PropertyName, float Value) const;

    FString GetStrProperty(FName PropertyName) const;
    void SetStrProperty(FName PropertyName, const FString Value) const;

    bool GetBoolProperty(FName PropertyName) const;
    void SetBoolProperty(FName PropertyName, bool Value) const;

    FName GetNameProperty(FName PropertyName) const;
    void SetNameProperty(FName PropertyName, FName Name) const;

    FText GetTextProperty(FName PropertyName) const;
    void SetTextProperty(FName PropertyName, const FText Text) const;

    UObject* GetObjectProperty(FName PropertyName) const;
    void SetObjectProperty(FName PropertyName, UObject* Object) const;

    FWeakObjectPtr GetWeakObjectProperty(FName PropertyName) const;
    void SetWeakObjectProperty(FName PropertyName, FWeakObjectPtr Object) const;

    FLazyObjectPtr GetLazyObjectProperty(FName PropertyName) const;
    void SetLazyObjectProperty(FName PropertyName, FLazyObjectPtr Object) const;

    FSoftObjectPtr GetSoftObjectProperty(FName PropertyName) const;
    void SetSoftObjectProperty(FName PropertyName, FSoftObjectPtr Object) const;

    FReflectedObject GetStructProperty(FName PropertyName) const;
    void SetStructProperty(FName PropertyName, const FReflectedObject& Struct) const;

    FReflectedObject GetArrayProperty(FName PropertyName) const;

    template<typename T>
    FORCEINLINE void SetStructProperty(FName PropertyName, const T& StructPtr) {
        FReflectedObject ReflectedObject{};
        ReflectedObject.SetupFromStruct(T::StaticStruct(), &StructPtr);
        SetStructProperty(PropertyName, ReflectedObject);
    }

    FReflectedEnumValue GetEnumProperty(FName PropertyName) const;
    void SetEnumProperty(FName PropertyName, const FReflectedEnumValue& Enum) const;
    // End UProperty accessors

    /** Exposes references to GC system */
    void AddStructReferencedObjects(class FReferenceCollector& Collector) const;

    /** Returns number of elements in this object, if it represents array. Otherwise returns 0 */
    int32 GetArrayNum() const;
    /** Adds new element into the array, and returns it's index. Returns -1 if this object does not represent array */
    int32 AddNewArrayElement() const;
    /** Removes specified number of elements starting at the provided index from the array, if this object represents an array */
    void RemoveArrayElements(int32 Index, int32 Count = 1) const;
    /** Clears array of all contained values, if this object represents an array */
    void ClearArray() const;

    /** Returns Struct this object is wrapping, or NULL if we are wrapping array of map */
    FORCEINLINE UStruct* GetStruct() const { return State.IsValid() ? State->GetStructObject() : NULL; }
private:
    template<typename T>
    FORCEINLINE T* FindPropertyByName(FName PropertyName, const bool bCheckWriteable) const {
        FProperty* Property = State.IsValid() ? State->FindPropertyByName(PropertyName) : NULL;
        if (T* CastedProperty = Cast<T>(Property)) {
            if (CastedProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
                if (bCheckWriteable) {
                    if (!CastedProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly)) {
                        return CastedProperty;
                    }
                } else {
                    return CastedProperty;
                }
            }
        }
        return NULL;
    }

    TSharedPtr<FReflectedObjectState> State;
};

template<>
struct SML_API TStructOpsTypeTraits<FReflectedObject> : public TStructOpsTypeTraitsBase2<FReflectedObject> {
    enum {
        WithCopy = true,
        WithAddStructReferencedObjects = true,
    };
};

class SML_API FReflectedObjectState_Array : public FReflectedObjectState {
public:
    FReflectedObjectState_Array(TSharedPtr<FReflectedObjectState> OwnerObject, FName ArrayPropertyName);

    virtual int32 GetArrayNum() const override;
    virtual int32 AddNewArrayElement() const override;
    virtual void RemoveArrayElements(int32 Index, int32 Count) const override;

    virtual TArray<FReflectedPropertyInfo> GetAllProperties() const override;
    virtual FProperty* FindPropertyByName(FName PropertyName) const override;
    virtual void* GetPropertyValue(FName PropertyName) override;
    virtual void AddReferencedObjects(FReferenceCollector& ReferenceCollector) override;
private:
    TSharedPtr<FReflectedObjectState> OwnerObjectState;
    FName ArrayPropertyName;
};

class SML_API FReflectedObjectState_UObject : public FReflectedObjectState {
public:
    FReflectedObjectState_UObject(UObject* Object);

    virtual UObject* GetObjectPointer() const override;
    virtual UStruct* GetStructObject() const override;
    virtual void* GetObjectData() override;
    virtual void AddReferencedObjects(FReferenceCollector& ReferenceCollector) override;
private:
    UObject* ReferencedObject;
};

class SML_API FReflectedObjectState_ScriptStruct : public FReflectedObjectState {
public:
    FReflectedObjectState_ScriptStruct(UScriptStruct* Struct);
    virtual ~FReflectedObjectState_ScriptStruct() override;

    virtual void CopyStructValue(UScriptStruct* StructType, void* StructValue) const override;
    virtual bool ShouldStripPropertyNames() const override;
    void CopyDataFromStruct(const void* SourceStruct) const;
    virtual UStruct* GetStructObject() const override;
    virtual void* GetObjectData() override;
    virtual void AddReferencedObjects(FReferenceCollector& ReferenceCollector) override;
private:
    UScriptStruct* ScriptStruct;
    void* StructDataPointer;
};
