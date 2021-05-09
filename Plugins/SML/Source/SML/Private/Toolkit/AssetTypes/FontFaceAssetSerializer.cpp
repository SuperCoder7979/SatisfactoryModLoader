#include "Toolkit/AssetTypes/FontFaceAssetSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Engine/FontFace.h"

void UFontFaceAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UFontFace)

        // What we do next depends on loading policy specified
        TArray64<uint8> FontRawData;
        if (Asset->LoadingPolicy == EFontLoadingPolicy::Inline) {
            // Font is inlined into this font face asset, retrieve data directly
            FontRawData = Asset->FontFaceData.Get().GetData();
        } else {
            // Font is saved inside of the cooked file, load the file by source file path
            FFileHelper::LoadFileToArray(FontRawData, *Asset->SourceFilename);
        }
        // Make sure we loaded data and dump it into destination file
        check(FontRawData.Num());

        // Theoretically .ufont can be any kind of font format that FreeType supports,
        // but since most of the programs (including UE importer and Windows font viewer) are able to
        // differentiate between TrueType and OpenType without looking at the extension, we just assume ttf format
        const FString ResultFontFilename = Context->GetDumpFilePath(TEXT(""), TEXT("ttf"));
        check(FFileHelper::SaveArrayToFile(FontRawData, *ResultFontFilename));

        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UFontFaceAssetSerializer::GetAssetClass() const {
    return UFontFace::StaticClass()->GetFName();
}
