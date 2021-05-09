#include "Toolkit/AssetTypes/StaticMeshAssetSerializer.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UStaticMeshAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UStaticMesh)

        // Just serialize normal properties into root object
        SERIALIZE_ASSET_OBJECT

        // Serialize extra properties using native serialization
        Data->SetNumberField(TEXT("MaxShadowLOD"), Asset->MaxShadowLOD);
        Data->SetStringField(TEXT("LightingGuid"), Asset->LightingGuid.ToString());
        Data->SetNumberField(TEXT("NavCollision"), ObjectSerializer->SerializeObject(Asset->NavCollision));

        // For some reason some UPROPERTY are duplicated in native serialization, e.g StaticMaterials and Sockets
        // we won't serialize them twice because there is literally no reason to do it

        // Serialize BodySetup since it is serialized natively
        const TSharedPtr<FJsonObject> BodySetupObject = SerializeBodySetup(Asset->BodySetup, ObjectSerializer);
        Data->SetObjectField(TEXT("BodySetup"), BodySetupObject);

        // Export raw mesh data into separate FBX file that can be imported back into UE
        const FString OutFbxMeshFileName = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
        FString OutErrorMessage;
        const bool bSuccess = FFbxMeshExporter::ExportStaticMeshIntoFbxFile(Asset, OutFbxMeshFileName, false, &OutErrorMessage);
        checkf(bSuccess, TEXT("Failed to export static mesh %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

    END_ASSET_SERIALIZATION
}

TSharedPtr<FJsonObject> UStaticMeshAssetSerializer::SerializeBodySetup(UBodySetup* BodySetup, UObjectHierarchySerializer* ObjectSerializer) {
    TSharedPtr<FJsonObject> BodySetupObject = MakeShareable(new FJsonObject());
    if (BodySetup != NULL) {
        // Serialize normal body setup properties
        ObjectSerializer->SerializeObjectPropertiesIntoObject(BodySetup, BodySetupObject);

        // We don't really serialize cooked complex collision data, but set the flag indicating if we had one
        BodySetupObject->SetBoolField(TEXT("bHasCookedCollisionData"), BodySetup->bHasCookedCollisionData);
    }
    return BodySetupObject;
}

FName UStaticMeshAssetSerializer::GetAssetClass() const {
    return UStaticMesh::StaticClass()->GetFName();
}

bool UStaticMeshAssetSerializer::SupportsParallelDumping() const {
    return false;
}
