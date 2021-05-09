#include "Toolkit/AssetTypes/AnimationBlueprintAssetSerializer.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UAnimationBlueprintAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION_BP(UAnimBlueprintGeneratedClass)
        DISABLE_SERIALIZATION(FExposedValueHandler, Function);
        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UAnimationBlueprintAssetSerializer::GetAssetClass() const {
    return TEXT("AnimBlueprint"); // UAnimBlueprint::StaticClass()->GetFName();
}
