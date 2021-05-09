#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "BlueprintAssetSerializer.generated.h"

UCLASS()
class SML_API UBlueprintAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    /** Serializes UBlueprintGeneratedClass instance */
    static void SerializeBlueprintClass(class UBlueprintGeneratedClass* Asset, TSharedPtr<class FJsonObject> Data, TSharedRef<FSerializationContext> Context);

    virtual FName GetAssetClass() const override;
};
