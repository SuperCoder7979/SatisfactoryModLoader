#include "Toolkit/AssetTypes/FontAssetSerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/TextureAssetSerializer.h"
#include "Engine/Font.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UFontAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UFont)

        // Textures need special treatment through texture asset serializer
        OVERRIDE_SERIALIZATION(
            UFont,
            Textures,
            {
            ARRAY_PROPERTY_VALUE(UTexture2D*);
            TArray<TSharedPtr<FJsonValue>> Textures;
            for (UTexture2D* FontTexture : PropertyValue) {
            const TSharedPtr<FJsonObject> TextureObject = MakeShareable(new FJsonObject());
            TextureObject->SetStringField(TEXT("TextureName"), *FontTexture->GetName());
            UTextureAssetSerializer::SerializeTexture2D(FontTexture, TextureObject, Context, FontTexture->GetName());
            Textures.Add(MakeShareable(new FJsonValueObject(TextureObject)));
            }
            return MakeShareable(new FJsonValueArray(Textures));
            }
        )

        // Serialize remap map if it is not empty
        TArray<TSharedPtr<FJsonValue>> CharacterRemapMap;
        for (const TPair<uint16, uint16>& Pair : Asset->CharRemap) {
            TSharedPtr<FJsonObject> Character = MakeShareable(new FJsonObject());
            Character->SetNumberField(TEXT("SrcChar"), Pair.Key);
            Character->SetNumberField(TEXT("DstChar"), Pair.Value);
            CharacterRemapMap.Add(MakeShareable(new FJsonValueObject(Character)));
        }
        if (Asset->IsRemapped) {
            Data->SetArrayField(TEXT("CharRemap"), CharacterRemapMap);
        }

        // Serialize normal asset data now
        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UFontAssetSerializer::GetAssetClass() const {
    return UFont::StaticClass()->GetFName();
}
