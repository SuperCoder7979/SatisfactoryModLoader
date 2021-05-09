#include "Toolkit/AssetTypes/MaterialParameterCollectionAssetSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Materials/MaterialParameterCollection.h"

void UMaterialParameterCollectionAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    // In fact, MPC is one of the simplest assets to serialize
    // It doesn't even have custom Serialize override with extra data, just normal UPROPERTY structs
    BEGIN_ASSET_SERIALIZATION(UMaterialParameterCollection)
        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UMaterialParameterCollectionAssetSerializer::GetAssetClass() const {
    return UMaterialParameterCollection::StaticClass()->GetFName();
}
