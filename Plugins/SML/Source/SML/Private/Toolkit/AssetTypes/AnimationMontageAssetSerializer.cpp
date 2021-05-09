#include "Toolkit/AssetTypes/AnimationMontageAssetSerializer.h"
#include "Animation/AnimMontage.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UAnimationMontageAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UAnimMontage)

        DISABLE_SERIALIZATION(UAnimSequenceBase, RawCurveData)
        check(Asset->RawCurveData.FloatCurves.Num() == 0);

        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UAnimationMontageAssetSerializer::GetAssetClass() const {
    return UAnimMontage::StaticClass()->GetFName();
}
