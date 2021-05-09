#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "SkeletonAssetSerializer.generated.h"

UCLASS()
class SML_API USkeletonAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    static void SerializeSkeleton(class USkeleton* Asset, TSharedPtr<class FJsonObject> Data, TSharedRef<FSerializationContext> Context);

    static void SerializeSmartNameContainer(const struct FSmartNameContainer& Container, TSharedPtr<class FJsonObject> OutObject);

    virtual FName GetAssetClass() const override;
    virtual bool SupportsParallelDumping() const override;
};
