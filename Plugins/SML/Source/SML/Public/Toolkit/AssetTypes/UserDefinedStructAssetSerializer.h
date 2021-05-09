#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "UserDefinedStructAssetSerializer.generated.h"

UCLASS()
class SML_API UUserDefinedStructAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
