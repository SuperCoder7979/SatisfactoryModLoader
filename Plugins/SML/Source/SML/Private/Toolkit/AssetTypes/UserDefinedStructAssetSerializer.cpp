#include "Toolkit/AssetTypes/UserDefinedStructAssetSerializer.h"
#include "Engine/UserDefinedStruct.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"

void UUserDefinedStructAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UUserDefinedStruct)
        FAssetHelper::SerializeScriptStruct(Data, Asset, ObjectSerializer);

        // Serialize status (should be up to date outside editor)
        Data->SetNumberField(TEXT("Status"), Asset->Status);
        Data->SetStringField(TEXT("Guid"), Asset->GetCustomGuid().ToString());

        // Serialize struct default values
        const TSharedRef<FJsonObject> DefaultInstance = Serializer->SerializeStruct(Asset, Asset->GetDefaultInstance());
        Data->SetObjectField(TEXT("StructDefaultInstance"), DefaultInstance);
    END_ASSET_SERIALIZATION
}

FName UUserDefinedStructAssetSerializer::GetAssetClass() const {
    return UUserDefinedStruct::StaticClass()->GetFName();
}
