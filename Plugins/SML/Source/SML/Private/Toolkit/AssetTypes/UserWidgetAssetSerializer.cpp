#include "Toolkit/AssetTypes/UserWidgetAssetSerializer.h"
#include "Blueprint/WidgetTree.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/BlueprintAssetSerializer.h"

void UUserWidgetAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION_BP(UWidgetBlueprintGeneratedClass)
        UBlueprintAssetSerializer::SerializeBlueprintClass(Asset, Data, Context);
    END_ASSET_SERIALIZATION
}

FName UUserWidgetAssetSerializer::GetAssetClass() const {
    return TEXT("WidgetBlueprint"); // UWidgetBlueprint::StaticClass()->GetFName();
}
