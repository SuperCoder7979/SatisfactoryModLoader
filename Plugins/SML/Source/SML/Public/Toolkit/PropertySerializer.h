#pragma once
#include "Dom/JsonObject.h"
#include "UObject/Object.h"
#include "PropertySerializer.generated.h"

class UObjectHierarchySerializer;

UCLASS()
class SML_API UPropertySerializer : public UObject {
    GENERATED_BODY()
private:
    friend class UObjectHierarchySerializer;
private:
    using FPropertySerializer = TFunction<TSharedRef<FJsonValue>(FProperty* Property, const void* Value)>;
    using FPropertyDeserializer = TFunction<void(FProperty* Property, TSharedRef<FJsonValue> Value, void* OutValue)>;

    UPROPERTY()
    UObjectHierarchySerializer* ObjectHierarchySerializer;

    UPROPERTY()
    TArray<UStruct*> PinnedStructs;

    TMap<FProperty*, FPropertySerializer> CustomPropertySerializers;
    TMap<FProperty*, FPropertyDeserializer> CustomPropertyDeserializers;
    TArray<FProperty*> BlacklistedProperties;
public:
    /** Disables property serialization entirely */
    void DisablePropertySerialization(UStruct* Struct, FName PropertyName);

    /** Sets up custom property serializer for provided property */
    void SetCustomSerializer(UStruct* Struct, FName PropertyName, FPropertySerializer Serializer);

    /** Sets up custom property deserializer for provided property */
    void SetCustomDeserializer(UStruct* Struct, FName PropertyName, FPropertyDeserializer Deserializer);

    /** Checks whenever we should serialize property in question at all */
    bool ShouldSerializeProperty(FProperty* Property) const;

    TSharedRef<FJsonValue> SerializePropertyValue(FProperty* Property, const void* Value);
    TSharedRef<FJsonObject> SerializeStruct(UScriptStruct* Struct, const void* Value);

    void DeserializePropertyValue(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
    void DeserializeStruct(UScriptStruct* Struct, const TSharedRef<FJsonObject>& Value, void* OutValue);
private:
    void DeserializePropertyValueInner(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
    TSharedRef<FJsonValue> SerializePropertyValueInner(FProperty* Property, const void* Value);
};
