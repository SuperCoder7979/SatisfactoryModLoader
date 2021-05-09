#pragma once

#define BEGIN_ASSET_SERIALIZATION(AssetClass) \
    { \
        AssetClass* Asset = Context->GetAsset<AssetClass>(); \
        TSharedRef<FJsonObject> Data = Context->GetData(); \
        UPropertySerializer* Serializer = Context->GetPropertySerializer(); \
        UObjectHierarchySerializer* ObjectSerializer = Context->GetObjectSerializer();

#define BEGIN_ASSET_SERIALIZATION_BP(AssetClass) \
{ \
    AssetClass* Asset = Context->GetBlueprintAsset<AssetClass>(); \
    TSharedRef<FJsonObject> Data = Context->GetData(); \
    UPropertySerializer* Serializer = Context->GetPropertySerializer(); \
    UObjectHierarchySerializer* ObjectSerializer = Context->GetObjectSerializer();

#define END_ASSET_SERIALIZATION \
    }

#define SERIALIZE_ASSET_OBJECT \
    { \
        TSharedRef<FJsonObject> AssetResultData = MakeShareable(new FJsonObject()); \
        ObjectSerializer->SerializeObjectPropertiesIntoObject(Asset, AssetResultData); \
        Data->SetObjectField(TEXT("AssetObjectData"), AssetResultData); \
    }

#define SERIALIZE_STRUCT_VALUE(Value) \
    Serializer->SerializeStruct(FAssetTypeSerializerMacros_Internals::GetScriptStruct<decltype(Value)>(), &Value)

#define SERIALIZE_PROPERTY_VALUE(ObjectPointer, PropertyName) \
    Serializer->SerializePropertyValue(FAssetTypeSerializerMacros_Internals::GetStaticStructOrClass<TRemovePointer<decltype(ObjectPointer)>::Type>()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(TRemovePointer<decltype(ObjectPointer)>::Type, PropertyName)), &ObjectPointer->PropertyName)

template<typename T>
struct FAssetTypeSerializerMacros_ToStringHelper;

template<>
struct FAssetTypeSerializerMacros_ToStringHelper<FString> {
    FORCEINLINE static FString ValueToString(const FString& SourceValue) { return SourceValue; }
};

template<>
struct FAssetTypeSerializerMacros_ToStringHelper<FName> {
    FORCEINLINE static FString ValueToString(const FName& SourceValue) { return SourceValue.ToString(); }
};

#define SERIALIZE_STRUCT_MAP(VariableName, MapValue) \
    TArray<TSharedPtr<FJsonValue>> VariableName; \
    for (const decltype(MapValue)::ElementType& Pair : MapValue) { \
        TSharedPtr<FJsonObject> PairObject = MakeShareable(new FJsonObject()); \
        PairObject->SetStringField(TEXT("Key"), FAssetTypeSerializerMacros_Internals::ValueToString<decltype(Pair.Key)>(Pair.Key)); \
        PairObject->SetObjectField(TEXT("Value"), SERIALIZE_STRUCT_VALUE(Pair.Value)); \
        VariableName.Add(MakeShareable(new FJsonValueObject(PairObject))); \
    }

struct FAssetTypeSerializerMacros_Internals {
private:
    template<typename T>
    static UStruct* GetStaticStructOrClass(std::true_type) {
        return T::StaticClass();
    }

    template<typename T>
    static UStruct* GetStaticStructOrClass(std::false_type) {
        return T::StaticStruct();
    }

public:
    template<typename T>
    static FString ValueToString(const T& SourceValue) {
        return FAssetTypeSerializerMacros_ToStringHelper<T>::ValueToString(SourceValue);
    }

    template<typename T>
    static UScriptStruct* GetScriptStruct() {
        return T::StaticStruct();
    }

    template<typename T>
    static UStruct* GetStaticStructOrClass() {
        return GetStaticStructOrClass<T>(std::is_base_of<UObject, T>());
    }
};

#define DISABLE_SERIALIZATION(Class, PropertyName) \
    Serializer->DisablePropertySerialization(FAssetTypeSerializerMacros_Internals::GetStaticStructOrClass<Class>(), GET_MEMBER_NAME_CHECKED(Class, PropertyName));

#define OVERRIDE_SERIALIZATION(Class, PropertyName, SerializerBody) \
    Serializer->SetCustomSerializer(FAssetTypeSerializerMacros_Internals::GetStaticStructOrClass<Class>(), GET_MEMBER_NAME_CHECKED(Class, PropertyName), \
        [=](FProperty* Property, const void* PropertyValue_Internal) { SerializerBody });

#define ARRAY_PROPERTY_VALUE(ElementType) const TArray<ElementType>& PropertyValue = *(const TArray<ElementType>*) PropertyValue_Internal;
#define ELEMENT_PROPERTY_VALUE(ElementType) const ElementType* PropertyValue = *(const ElementType*) PropertyValue_Internal;
