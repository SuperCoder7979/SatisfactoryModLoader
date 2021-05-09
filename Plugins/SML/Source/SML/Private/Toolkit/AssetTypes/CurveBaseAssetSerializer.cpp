#include "Toolkit/AssetTypes/CurveBaseAssetSerializer.h"
#include "Curves/CurveBase.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UCurveBaseAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UCurveBase)
        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

void UCurveBaseAssetSerializer::GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) {
    OutExtraAssetClasses.Add(UCurveVector::StaticClass()->GetFName());
    OutExtraAssetClasses.Add(UCurveFloat::StaticClass()->GetFName());
    OutExtraAssetClasses.Add(UCurveLinearColor::StaticClass()->GetFName());
}

FName UCurveBaseAssetSerializer::GetAssetClass() const {
    return UCurveBase::StaticClass()->GetFName();
}
