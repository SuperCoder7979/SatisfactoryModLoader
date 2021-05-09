#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "UserWidgetAssetSerializer.generated.h"

UCLASS()
class SML_API UUserWidgetAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
