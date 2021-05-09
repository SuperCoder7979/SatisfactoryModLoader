#include "Toolkit/AssetTypes/DataTableAssetSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Engine/DataTable.h"

void UDataTableAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UDataTable)

        const TSharedPtr<FJsonObject> RowData = MakeShareable(new FJsonObject());
        const TMap<FName, uint8*>& RowDataMap = Asset->GetRowMap();
        for (const TPair<FName, uint8*>& RowDataPair : RowDataMap) {
            TSharedPtr<FJsonObject> StructData = Serializer->SerializeStruct(Asset->RowStruct, RowDataPair.Value);
            RowData->SetObjectField(RowDataPair.Key.ToString(), StructData);
        }

        Data->SetNumberField(TEXT("RowStruct"), ObjectSerializer->SerializeObject(Asset->RowStruct));
        Data->SetObjectField(TEXT("RowData"), RowData);

    END_ASSET_SERIALIZATION
}

FName UDataTableAssetSerializer::GetAssetClass() const {
    return UDataTable::StaticClass()->GetFName();
}
