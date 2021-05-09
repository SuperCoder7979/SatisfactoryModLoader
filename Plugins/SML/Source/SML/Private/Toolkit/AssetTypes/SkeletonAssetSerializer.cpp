#include "Toolkit/AssetTypes/SkeletonAssetSerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Toolkit/PropertySerializer.h"
#include "Reflection/ReflectionHelper.h"
#include "Toolkit/AssetTypes/SkeletalMeshAssetSerializer.h"
#include "Animation/Skeleton.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void USkeletonAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(USkeleton)

        // Serialize reference skeleton object
        const TSharedPtr<FJsonObject> ReferenceSkeleton = MakeShareable(new FJsonObject());
        USkeletalMeshAssetSerializer::SerializeReferenceSkeleton(Asset->GetReferenceSkeleton(), ReferenceSkeleton);
        Data->SetObjectField(TEXT("ReferenceSkeleton"), ReferenceSkeleton);

        // Serialize Skeleton GUID, good idea to keep it consistent with cooked data
        Data->SetStringField(TEXT("Guid"), Asset->GetGuid().ToString());

        // Serialize animation retarget sources
        SERIALIZE_STRUCT_MAP(AnimRetargetSources, Asset->AnimRetargetSources);
        Data->SetArrayField(TEXT("AnimRetargetSources"), AnimRetargetSources);

        // Serialize smart names container (in this ugly way because field is private)
        Serializer->DisablePropertySerialization(USkeleton::StaticClass(), TEXT("SmartNames"));
        FProperty* SmartNamesProperty = USkeleton::StaticClass()->FindPropertyByName(TEXT("SmartNames"));

        const TSharedPtr<FJsonObject> NameContainer = MakeShareable(new FJsonObject());
        SerializeSmartNameContainer(*SmartNamesProperty->ContainerPtrToValuePtr<FSmartNameContainer>(Asset), NameContainer);
        Data->SetObjectField(TEXT("SmartNames"), NameContainer);

        // Serialize normal asset data
        SERIALIZE_ASSET_OBJECT

        // Serialize skeleton itself into the fbx file
        const FString OutFbxFilename = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
        FString OutErrorMessage;
        const bool bSuccess = FFbxMeshExporter::ExportSkeletonIntoFbxFile(Asset, OutFbxFilename, false, &OutErrorMessage);
        checkf(bSuccess, TEXT("Failed to export skeleton %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

    END_ASSET_SERIALIZATION
}

void USkeletonAssetSerializer::SerializeSkeleton(USkeleton* Asset, TSharedPtr<FJsonObject> Data, TSharedRef<FSerializationContext> Context) {
    UPropertySerializer* Serializer = Context->GetPropertySerializer();
    UObjectHierarchySerializer* ObjectSerializer = Context->GetObjectSerializer();

    // Serialize reference skeleton object
    const TSharedPtr<FJsonObject> ReferenceSkeleton = MakeShareable(new FJsonObject());
    USkeletalMeshAssetSerializer::SerializeReferenceSkeleton(Asset->GetReferenceSkeleton(), ReferenceSkeleton);
    Data->SetObjectField(TEXT("ReferenceSkeleton"), ReferenceSkeleton);

    // Serialize Skeleton GUID, good idea to keep it consistent with cooked data
    Data->SetStringField(TEXT("Guid"), Asset->GetGuid().ToString());

    // Serialize animation retarget sources
    SERIALIZE_STRUCT_MAP(AnimRetargetSources, Asset->AnimRetargetSources);
    Data->SetArrayField(TEXT("AnimRetargetSources"), AnimRetargetSources);

    Serializer->DisablePropertySerialization(USkeleton::StaticClass(), TEXT("SmartNames"));
    FProperty* SmartNamesProperty = USkeleton::StaticClass()->FindPropertyByName(TEXT("SmartNames"));
    const FSmartNameContainer* SmartNameContainer = SmartNamesProperty->ContainerPtrToValuePtr<FSmartNameContainer>(Asset);

    const TSharedPtr<FJsonObject> NameContainer = MakeShareable(new FJsonObject());
    SerializeSmartNameContainer(*SmartNameContainer, NameContainer);
    Data->SetObjectField(TEXT("SmartNames"), NameContainer);

    SERIALIZE_ASSET_OBJECT

    // Serialize skeleton itself into the fbx file
    const FString OutFbxFilename = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
    FString OutErrorMessage;
    const bool bSuccess = FFbxMeshExporter::ExportSkeletonIntoFbxFile(Asset, OutFbxFilename, false, &OutErrorMessage);
    checkf(bSuccess, TEXT("Failed to export skeleton %s: %s"), *Asset->GetPathName(), *OutErrorMessage);
}

void USkeletonAssetSerializer::SerializeSmartNameContainer(const FSmartNameContainer& Container, TSharedPtr<FJsonObject> OutObject) {
    // Serialize smart name mappings
    TArray<TSharedPtr<FJsonValue>> NameMappings;
    for (FName SmartNameId : Container.GetMappingNames()) {
        const FSmartNameMapping* Mapping = Container.GetContainer(SmartNameId);

        TSharedPtr<FJsonObject> NameMapping = MakeShareable(new FJsonObject());
        NameMapping->SetStringField(TEXT("Name"), SmartNameId.ToString());

        TArray<FName> MetaDataKeys;
        Mapping->FillUIDToNameArray(MetaDataKeys);

        TArray<TSharedPtr<FJsonValue>> CurveMetaDataMap;

        for (const FName& MetaDataKey : MetaDataKeys) {
            const TSharedPtr<FJsonObject> MetaDataObject = MakeShareable(new FJsonObject());
            const FCurveMetaData* MetaData = Mapping->GetCurveMetaData(MetaDataKey);

            MetaDataObject->SetStringField(TEXT("MetaDataKey"), MetaDataKey.ToString());
            MetaDataObject->SetBoolField(TEXT("bMaterial"), MetaData->Type.bMaterial);
            MetaDataObject->SetBoolField(TEXT("bMorphtarget"), MetaData->Type.bMorphtarget);
            MetaDataObject->SetNumberField(TEXT("MaxLOD"), MetaData->MaxLOD);

            TArray<TSharedPtr<FJsonValue>> LinkedBones;
            for (const FBoneReference& BoneReference : MetaData->LinkedBones) {
                LinkedBones.Add(MakeShareable(new FJsonValueString(BoneReference.BoneName.ToString())));
            }

            MetaDataObject->SetArrayField(TEXT("LinkedBones"), LinkedBones);
            CurveMetaDataMap.Add(MakeShareable(new FJsonValueObject(MetaDataObject)));
        }

        NameMapping->SetArrayField(TEXT("CurveMetaDataMap"), CurveMetaDataMap);
        NameMappings.Add(MakeShareable(new FJsonValueObject(NameMapping)));
    }
    OutObject->SetArrayField(TEXT("NameMappings"), NameMappings);
}

FName USkeletonAssetSerializer::GetAssetClass() const {
    return USkeleton::StaticClass()->GetFName();
}

bool USkeletonAssetSerializer::SupportsParallelDumping() const {
    return false;
}
