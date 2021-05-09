#include "Toolkit/AssetTypes/AnimationSequenceAssetSerializer.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Toolkit/PropertySerializer.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"

void UAnimationSequenceAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UAnimSequence)

        // Disable serialization of RawCurveData. It will always be empty during cooking anyway, UAnimSequence class ensures that it is the case.
        DISABLE_SERIALIZATION(UAnimSequenceBase, RawCurveData)
        check(Asset->RawCurveData.FloatCurves.Num() == 0);

        SERIALIZE_ASSET_OBJECT

        // Serialize animation data
        const FString OutFbxFileName = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
        FString OutErrorMessage;
        const bool bSuccess = FFbxMeshExporter::ExportAnimSequenceIntoFbxFile(Asset, OutFbxFileName, false, &OutErrorMessage);
        checkf(bSuccess, TEXT("Failed to export anim sequence %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

    END_ASSET_SERIALIZATION
}

FName UAnimationSequenceAssetSerializer::GetAssetClass() const {
    return UAnimSequence::StaticClass()->GetFName();
}

bool UAnimationSequenceAssetSerializer::SupportsParallelDumping() const {
    return false;
}
