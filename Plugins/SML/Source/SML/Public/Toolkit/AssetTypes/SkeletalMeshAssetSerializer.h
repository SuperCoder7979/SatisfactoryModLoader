#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "SkeletalMeshAssetSerializer.generated.h"

UCLASS()
class SML_API USkeletalMeshAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    static void SerializeReferenceSkeleton(const struct FReferenceSkeleton& ReferenceSkeleton, TSharedPtr<class FJsonObject> OutObject);

    virtual FName GetAssetClass() const override;
    virtual bool SupportsParallelDumping() const override;
};
