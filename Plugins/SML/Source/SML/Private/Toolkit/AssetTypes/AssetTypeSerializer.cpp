#include "Toolkit/AssetDumping/AssetTypeSerializer.h"

// Static class used to lazily populate serializer registry
class FAssetTypeSerializerRegistry {
public:
    // Native classes should never get unloaded anyway, so we can use TWeakObjectPtr safely
    TMap<FName, TWeakObjectPtr<UAssetTypeSerializer>> Serializers;
    TArray<TWeakObjectPtr<UAssetTypeSerializer>> SerializersArray;

    // Constructor that will automatically populate registry serializers
    FAssetTypeSerializerRegistry();

    static const FAssetTypeSerializerRegistry& Get() {
        static FAssetTypeSerializerRegistry AssetTypeSerializerRegistry{};
        return AssetTypeSerializerRegistry;
    }
};

UAssetTypeSerializer* UAssetTypeSerializer::FindSerializerForAssetClass(const FName AssetClass) {
    const FAssetTypeSerializerRegistry& Registry = FAssetTypeSerializerRegistry::Get();
    const TWeakObjectPtr<UAssetTypeSerializer>* Serializer = Registry.Serializers.Find(AssetClass);
    return Serializer ? Serializer->Get() : NULL;
}

TArray<UAssetTypeSerializer*> UAssetTypeSerializer::GetAvailableAssetSerializers() {
    const FAssetTypeSerializerRegistry& Registry = FAssetTypeSerializerRegistry::Get();
    TArray<UAssetTypeSerializer*> Serializers;
    for (const TWeakObjectPtr<UAssetTypeSerializer>& Serializer : Registry.SerializersArray) {
        Serializers.Add(Serializer.Get());
    }
    return Serializers;
}

FAssetTypeSerializerRegistry::FAssetTypeSerializerRegistry() {
    TArray<UClass*> AssetSerializerClasses;
    GetDerivedClasses(UAssetTypeSerializer::StaticClass(), AssetSerializerClasses, true);

    // Iterate classes in memory to resolve serializers
    TArray<UAssetTypeSerializer*> OutFoundInitializers;
    for (UClass* Class : AssetSerializerClasses) {
        // Skip classes that are marked as Abstract
        if (Class->HasAnyClassFlags(CLASS_Abstract)) {
            continue;
        }
        // Skip classes that are not marked as Native
        if (!Class->HasAnyClassFlags(CLASS_Native)) {
            continue;
        }

        // Check asset type in class default object
        UAssetTypeSerializer* ClassDefaultObject = CastChecked<UAssetTypeSerializer>(Class->GetDefaultObject());
        if (ClassDefaultObject->GetAssetClass() != NAME_None) {
            OutFoundInitializers.Add(ClassDefaultObject);
        }
    }

    // First register additional asset classes, so primary ones will overwrite them later
    for (UAssetTypeSerializer* Serializer : OutFoundInitializers) {
        TArray<FName> OutExtraAssetClasses;
        Serializer->GetAdditionallyHandledAssetClasses(OutExtraAssetClasses);
        for (const FName& AssetClass : OutExtraAssetClasses) {
            this->Serializers.Add(AssetClass, Serializer);
        }
    }

    // Now setup primary asset classes and add serializers into array
    for (UAssetTypeSerializer* Serializer : OutFoundInitializers) {
        const FName AssetClass = Serializer->GetAssetClass();
        this->Serializers.Add(AssetClass, Serializer);
        this->SerializersArray.Add(Serializer);
    }
}
