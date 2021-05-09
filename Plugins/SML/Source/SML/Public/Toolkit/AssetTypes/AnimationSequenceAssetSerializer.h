#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "AnimationSequenceAssetSerializer.generated.h"

UCLASS()
class SML_API UAnimationSequenceAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
    virtual bool SupportsParallelDumping() const override;
};
