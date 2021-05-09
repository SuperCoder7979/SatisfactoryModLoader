#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "TextureCubeAssetSerializer.generated.h"

UCLASS()
class SML_API UTextureCubeAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    /** Serializes TextureCube, including exporting it to image file saved alongside json */
    static void SerializeTextureCube(class UTextureCube* Asset, TSharedPtr<class FJsonObject> Data, TSharedRef<FSerializationContext> Context);

    virtual FName GetAssetClass() const override;
};
