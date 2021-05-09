#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"

FSerializationContext::FSerializationContext(const FString& RootOutputDirectory, const FAssetData& AssetData, UPackage* Package) {
    this->AssetSerializedData = MakeShareable(new FJsonObject());
    this->PropertySerializer = NewObject<UPropertySerializer>();
    this->ObjectHierarchySerializer = NewObject<UObjectHierarchySerializer>();
    this->ObjectHierarchySerializer->SetPropertySerializer(PropertySerializer);

    this->PropertySerializer->AddToRoot();
    this->ObjectHierarchySerializer->AddToRoot();

    // Object hierarchy serializer will also root package object by referencing it
    this->ObjectHierarchySerializer->InitializeForSerialization(Package);
    this->Package = Package;
    this->AssetData = AssetData;

    this->RootOutputDirectory = RootOutputDirectory;
    this->PackageBaseDirectory = FPaths::Combine(RootOutputDirectory, AssetData.PackagePath.ToString());

    // Make sure package base directory exists
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*PackageBaseDirectory);

    // Retrieve universal asset object
    UObject* AssetObject;

    // Check whenever asset represents a blueprint, then we want to work with blueprint asset specific methods
    if (AssetData.TagsAndValues.Contains(FBlueprintTags::GeneratedClassPath)) {
        AssetObject = GetBlueprintAsset<UBlueprintGeneratedClass>();
    } else {
        // No GeneratedClassPath, it is an ordinary asset object
        AssetObject = GetAsset<UObject>();
    }

    // Set mark on the asset object so it can be referenced by other objects in hierarchy
    this->ObjectHierarchySerializer->SetObjectMark(AssetObject, TEXT("$AssetObject$"));
}

FSerializationContext::~FSerializationContext() {
    this->PropertySerializer->RemoveFromRoot();
    this->ObjectHierarchySerializer->RemoveFromRoot();

    this->PropertySerializer->MarkPendingKill();
    this->ObjectHierarchySerializer->MarkPendingKill();
}

void FSerializationContext::Finalize() const {
    TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    RootObject->SetStringField(TEXT("AssetClass"), AssetData.AssetClass.ToString());
    RootObject->SetStringField(TEXT("AssetPackage"), Package->GetName());
    RootObject->SetStringField(TEXT("AssetName"), AssetData.AssetName.ToString());

    RootObject->SetObjectField(TEXT("AssetSerializedData"), AssetSerializedData);
    RootObject->SetArrayField(TEXT("ObjectHierarchy"), ObjectHierarchySerializer->FinalizeSerialization());

    FString ResultString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
    FJsonSerializer::Serialize(RootObject, Writer);

    const FString OutputFilename = GetDumpFilePath(TEXT(""), TEXT("json"));
    check(FFileHelper::SaveStringToFile(ResultString, *OutputFilename));
}
