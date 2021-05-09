#include "Toolkit/AssetTypes/StringTableAssetSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"

void UStringTableAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UStringTable)

        const FStringTableConstRef StringTablePtr = Asset->GetStringTable();
        Data->SetStringField(TEXT("TableNamespace"), StringTablePtr->GetNamespace());

        TSharedPtr<FJsonObject> SourceStrings = MakeShareable(new FJsonObject());
        StringTablePtr->EnumerateSourceStrings(
            [&](const FString& InKey, const FString& DisplayString) {
                SourceStrings->SetStringField(InKey, DisplayString);
                return true;
            }
        );
        Data->SetObjectField(TEXT("SourceStrings"), SourceStrings);

        TArray<FString> InStringTableKeys;
        SourceStrings->Values.GetKeys(InStringTableKeys);

        TSharedPtr<FJsonObject> KeyMetaData = MakeShareable(new FJsonObject());
        for (const FString& InKey : InStringTableKeys) {
            TSharedPtr<FJsonObject> MetaDataObject = MakeShareable(new FJsonObject());
            StringTablePtr->EnumerateMetaData(
                InKey,
                [&](FName MetaDataKey, const FString& Value) {
                    MetaDataObject->SetStringField(MetaDataKey.ToString(), Value);
                    return true;
                }
            );
            if (MetaDataObject->Values.Num()) {
                KeyMetaData->SetObjectField(InKey, MetaDataObject);
            }
        }
        Data->SetObjectField(TEXT("MetaData"), KeyMetaData);

    END_ASSET_SERIALIZATION
}

FName UStringTableAssetSerializer::GetAssetClass() const {
    return UStringTable::StaticClass()->GetFName();
}
