#include "Toolkit/AssetTypes/BlueprintAssetSerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "Toolkit/PropertySerializer.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UBlueprintAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION_BP(UBlueprintGeneratedClass)
        SerializeBlueprintClass(Asset, Data, Context);
    END_ASSET_SERIALIZATION
}

void UBlueprintAssetSerializer::SerializeBlueprintClass(UBlueprintGeneratedClass* Asset, TSharedPtr<FJsonObject> Data, TSharedRef<FSerializationContext> Context) {
    UPropertySerializer* Serializer = Context->GetPropertySerializer();
    UObjectHierarchySerializer* ObjectSerializer = Context->GetObjectSerializer();

    // Serialize parent UClass object first
    FAssetHelper::SerializeClass(Data, Asset, ObjectSerializer);

    DISABLE_SERIALIZATION(USCS_Node, CookedComponentInstancingData);
    DISABLE_SERIALIZATION(UBlueprintGeneratedClass, CookedComponentInstancingData);
    DISABLE_SERIALIZATION(UBlueprintGeneratedClass, UberGraphFunction);

    if (Asset->UberGraphFunction != NULL) {
        Data->SetStringField(TEXT("UberGraphFunction"), Asset->UberGraphFunction->GetName());
        Data->SetStringField(TEXT("UberGraphFramePointerProperty"), Asset->UberGraphFramePointerProperty->GetName());
    }

    // Serialize class default object (to be fair, it will be serialized already at this point anyway)
    const int32 ClassDefaultObjectIndex = ObjectSerializer->SerializeObject(Asset->GetDefaultObject());
    Data->SetNumberField(TEXT("ClassDefaultObject"), ClassDefaultObjectIndex);

    SERIALIZE_ASSET_OBJECT
}

FName UBlueprintAssetSerializer::GetAssetClass() const {
    return TEXT("Blueprint"); // UBlueprint::StaticClass()->GetFName()
}
