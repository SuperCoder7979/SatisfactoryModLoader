#include "Toolkit/AssetTypes/SkeletalMeshAssetSerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/StaticMeshAssetSerializer.h"
#include "Engine/SkeletalMesh.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void USkeletalMeshAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(USkeletalMesh)

        // Serialize normal asset data
        SERIALIZE_ASSET_OBJECT

        // Serialize few properties that are marked as transient but in fact are serialized natively (probably for cooking/compatibility reasons)
        Data->SetField(TEXT("Materials"), SERIALIZE_PROPERTY_VALUE(Asset, Materials));

        // Serialize reference skeleton
        const TSharedPtr<FJsonObject> ReferenceSkeleton = MakeShareable(new FJsonObject());
        SerializeReferenceSkeleton(Asset->RefSkeleton, ReferenceSkeleton);
        Data->SetObjectField(TEXT("RefSkeleton"), ReferenceSkeleton);

        // Serialize BodySetup if per poly collision is enabled
        if (Asset->bEnablePerPolyCollision) {
            const TSharedPtr<FJsonObject> BodySetupObject = UStaticMeshAssetSerializer::SerializeBodySetup(Asset->BodySetup, ObjectSerializer);
            Data->SetObjectField(TEXT("BodySetup"), BodySetupObject);
        }

        // Export raw mesh data into separate FBX file that can be imported back into UE
        const FString OutFbxMeshFileName = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
        FString OutErrorMessage;
        const bool bSuccess = FFbxMeshExporter::ExportSkeletalMeshIntoFbxFile(Asset, OutFbxMeshFileName, false, &OutErrorMessage);
        checkf(bSuccess, TEXT("Failed to export skeletal mesh %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

    END_ASSET_SERIALIZATION
}

void USkeletalMeshAssetSerializer::SerializeReferenceSkeleton(const FReferenceSkeleton& ReferenceSkeleton, TSharedPtr<FJsonObject> OutObject) {
    // Serialize pose together with bone info
    TArray<TSharedPtr<FJsonValue>> SkeletonBones;
    for (int32 i = 0; i < ReferenceSkeleton.GetRawBoneNum(); i++) {
        const FMeshBoneInfo& BoneInfo = ReferenceSkeleton.GetRawRefBoneInfo()[i];
        const FTransform& PoseTransform = ReferenceSkeleton.GetRawRefBonePose()[i];

        TSharedPtr<FJsonObject> BoneObject = MakeShareable(new FJsonObject());
        BoneObject->SetStringField(TEXT("Name"), BoneInfo.Name.ToString());
        BoneObject->SetNumberField(TEXT("ParentIndex"), BoneInfo.ParentIndex);
        BoneObject->SetNumberField(TEXT("Index"), i);
        BoneObject->SetStringField(TEXT("Pose"), PoseTransform.ToString());

        SkeletonBones.Add(MakeShareable(new FJsonValueObject(BoneObject)));
    }
    OutObject->SetArrayField(TEXT("Bones"), SkeletonBones);

    // Do not serialize RawNameToIndexMap because it can be reconstructed
    // from serialize bone info structure
}

FName USkeletalMeshAssetSerializer::GetAssetClass() const {
    return USkeletalMesh::StaticClass()->GetFName();
}

bool USkeletalMeshAssetSerializer::SupportsParallelDumping() const {
    return false;
}
