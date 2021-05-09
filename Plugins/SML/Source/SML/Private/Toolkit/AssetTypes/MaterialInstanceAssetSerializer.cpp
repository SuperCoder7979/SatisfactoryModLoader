#include "Toolkit/AssetTypes/MaterialInstanceAssetSerializer.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Toolkit/PropertySerializer.h"

void UMaterialInstanceAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UMaterialInstance)
        SERIALIZE_ASSET_OBJECT

        // Serialize shader maps if this material instance has static permutations
        Data->SetBoolField(TEXT("HasStaticPermutationResource"), Asset->bHasStaticPermutationResource);

        // TODO uncomment once we rework material bytecode serialization for UE4.25
        /*if (Asset->bHasStaticPermutationResource) {
            // Gather all unique material resources
            TSet<FMaterialResource*> MaterialResources;
            for (int32 FeatureLevel = 0; FeatureLevel < ERHIFeatureLevel::Num; FeatureLevel++) {
                for (int32 QualityLevel = 0; QualityLevel < EMaterialQualityLevel::Num; QualityLevel++) {
                    FMaterialResource* MaterialResource = Asset->GetMaterialResource((ERHIFeatureLevel::Type) FeatureLevel, EMaterialQualityLevel::High);
                    if (MaterialResource != NULL && MaterialResource->GetGameThreadShaderMap() != NULL) {
                        MaterialResources.Add(MaterialResource);
                    }
                }
            }

            check(MaterialResources.Num() > 0);

            // Dump material resources general information into json and shader bytecode into binary files
            TArray<TSharedPtr<FJsonValue>> MaterialResourcesArray;
            for (FMaterialResource* MaterialResource : MaterialResources) {
                const TSharedPtr<FJsonObject> ResourceObject = MakeShareable(new FJsonObject());
                UMaterialAssetSerializer::SerializeMaterialResource(MaterialResource, ResourceObject, ObjectHierarchySerializer, SerializationContext);
                MaterialResourcesArray.Add(MakeShareable(new FJsonValueObject(ResourceObject)));
            }
            Data->SetArrayField(TEXT("MaterialResources"), MaterialResourcesArray);
        }*/
    END_ASSET_SERIALIZATION
}

FName UMaterialInstanceAssetSerializer::GetAssetClass() const {
    return UMaterialInstanceConstant::StaticClass()->GetFName();
}
