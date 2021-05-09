#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "TextureAssetSerializer.generated.h"

UCLASS()
class SML_API UTextureAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    /** Serializes actual texture payload into provided serialization context. Set bResetAlpha to true to make entire image opaque and force alpha to 1.0f (used for cubemaps) */
    static void SerializeTextureData(const FString& ContextString, struct FTexturePlatformData* PlatformData, TSharedPtr<class FJsonObject> Data, TSharedRef<FSerializationContext> Context, bool bResetAlpha, const FString& FileNamePostfix);

    /** Serializes Texture2D, including exporting it to image file saved alongside json */
    static void SerializeTexture2D(class UTexture2D* Asset, TSharedPtr<class FJsonObject> Data, TSharedRef<class FSerializationContext> Context, const FString& Postfix);

    virtual FName GetAssetClass() const override;
};
