#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "StaticMeshAssetSerializer.generated.h"

UCLASS()
class SML_API UStaticMeshAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    static TSharedPtr<class FJsonObject> SerializeBodySetup(class UBodySetup* BodySetup, class UObjectHierarchySerializer* ObjectHierarchySerializer);

    virtual FName GetAssetClass() const override;
    virtual bool SupportsParallelDumping() const override;
};
