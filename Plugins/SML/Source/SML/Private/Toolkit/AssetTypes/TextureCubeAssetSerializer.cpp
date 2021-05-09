#include "Toolkit/AssetTypes/TextureCubeAssetSerializer.h"
#include "Engine/TextureCube.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/TextureAssetSerializer.h"

void UTextureCubeAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UTextureCube)
        SerializeTextureCube(Asset, Data, Context);
    END_ASSET_SERIALIZATION
}

void UTextureCubeAssetSerializer::SerializeTextureCube(UTextureCube* Asset, TSharedPtr<FJsonObject> Data, TSharedRef<FSerializationContext> Context) {
    UObjectHierarchySerializer* ObjectSerializer = Context->GetObjectSerializer();
    SERIALIZE_ASSET_OBJECT
    UTextureAssetSerializer::SerializeTextureData(Asset->GetPathName(), Asset->PlatformData, Data, Context, true, TEXT(""));
}

FName UTextureCubeAssetSerializer::GetAssetClass() const {
    return UTextureCube::StaticClass()->GetFName();
}
