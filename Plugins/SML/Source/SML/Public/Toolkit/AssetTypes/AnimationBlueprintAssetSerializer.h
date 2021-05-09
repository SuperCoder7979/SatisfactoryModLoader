#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Toolkit/AssetTypes/BlueprintAssetSerializer.h"
#include "AnimationBlueprintAssetSerializer.generated.h"

UCLASS()
class SML_API UAnimationBlueprintAssetSerializer : public UBlueprintAssetSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
