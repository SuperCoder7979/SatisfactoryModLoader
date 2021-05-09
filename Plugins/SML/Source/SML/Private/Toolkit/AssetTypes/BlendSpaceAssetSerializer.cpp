#include "Toolkit/AssetTypes/BlendSpaceAssetSerializer.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/BlendSpaceBase.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UBlendSpaceSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UBlendSpaceBase)
        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

void UBlendSpaceSerializer::GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) {
    OutExtraAssetClasses.Add(UBlendSpace::StaticClass()->GetFName());
    OutExtraAssetClasses.Add(UBlendSpace1D::StaticClass()->GetFName());
    OutExtraAssetClasses.Add(UAimOffsetBlendSpace::StaticClass()->GetFName());
    OutExtraAssetClasses.Add(UAimOffsetBlendSpace1D::StaticClass()->GetFName());
}

FName UBlendSpaceSerializer::GetAssetClass() const {
    return UBlendSpaceBase::StaticClass()->GetFName();
}
