#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "UserDefinedEnumAssetSerializer.generated.h"

UCLASS()
class SML_API UUserDefinedEnumAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
