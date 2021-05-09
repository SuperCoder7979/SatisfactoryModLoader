#pragma once
#include "UObject/Object.h"
#include "Json.h"
#include "ObjectHierarchySerializer.generated.h"

class UPropertySerializer;

UCLASS()
class SML_API UObjectHierarchySerializer : public UObject {
    GENERATED_BODY()
private:
    UPROPERTY()
    UPackage* SourcePackage;
    UPROPERTY()
    TMap<UObject*, int32> ObjectIndices;
    UPROPERTY()
    TMap<int32, UObject*> LoadedObjects;
    int32 LastObjectIndex;
    UPROPERTY()
    UPropertySerializer* PropertySerializer;
    TMap<int32, TSharedPtr<FJsonObject>> SerializedObjects;
    UPROPERTY()
    TArray<UClass*> AllowedNativeSerializeClasses;

    UPROPERTY()
    TMap<UObject*, FString> ObjectMarks;

    bool bAllowExportObjectSerialization;
public:
    UObjectHierarchySerializer();

    TSharedRef<FJsonObject> SerializeObjectProperties(UObject* Object);
    void SerializeObjectPropertiesIntoObject(UObject* Object, TSharedPtr<FJsonObject> OutObject);

    void DeserializeObjectProperties(const TSharedRef<FJsonObject>& Properties, UObject* Object);

    void SetPropertySerializer(UPropertySerializer* NewPropertySerializer);

    void InitializeForSerialization(UPackage* NewSourcePackage);

    /** Allows serialization of class with native Serialize override */
    void AllowNativeClassSerialization(UClass* ClassToAllow);

    /**
     * Sets object mark for provided object instance
     * Instances of this object will be serialized as a simple object mark string
     * During deserialization, it is used to lookup object by mark
     */
    void SetObjectMark(UObject* Object, const FString& ObjectMark);

    /**
     * Disables or enables exported object serialization
     * When exported object serialization is disabled, attempting to serialize
     * an object inside of the same package will trigger an exception
     */
    void SetAllowExportedObjectSerialization(bool bAllowExportedObjectSerialization);

    void InitializeForDeserialization(const TArray<TSharedPtr<FJsonValue>>& ObjectsArray);
    void SetPackageForDeserialization(UPackage* SelfPackage);

    UObject* DeserializeObject(int32 Index);

    int32 SerializeObject(UObject* Object);

    TArray<TSharedPtr<FJsonValue>> FinalizeSerialization();

    FORCEINLINE static const TSet<FName>& GetUnhandledNativeClasses() { return UnhandledNativeClasses; }
private:
    static TSet<FName> UnhandledNativeClasses;

    void SerializeImportedObject(TSharedPtr<FJsonObject> ResultJson, UObject* Object);
    void SerializeExportedObject(TSharedPtr<FJsonObject> ResultJson, UObject* Object);

    UObject* DeserializeImportedObject(TSharedPtr<FJsonObject> ObjectJson);
    UObject* DeserializeExportedObject(TSharedPtr<FJsonObject> ObjectJson);
};
