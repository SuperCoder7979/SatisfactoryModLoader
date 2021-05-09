#pragma once
#include "CoreMinimal.h"

class UPropertySerializer;
class UObjectHierarchySerializer;
class FJsonObject;

/**
 * Describes context used for the serialization of a single asset object
 * Contains some facilities for making serialization easier and
 * handling UObject hierarchies serialization, plus minimal metadata for
 * dependency listing and version for asset generation to read
 */
class SML_API FSerializationContext : public TSharedFromThis<FSerializationContext> {
private:
    friend class FAssetDumpProcessor;

    /** Root directory for asset dumping results */
    FString RootOutputDirectory;
    /** Directory under root directory where files related to this assets will be placed */
    FString PackageBaseDirectory;

    /** Original data for the asset we're dumping, as retrieved from asset registry */
    FAssetData AssetData;
    /** Package we are loading */
    UPackage* Package;

    /** Property serializer handling serialization of object properties */
    UPropertySerializer* PropertySerializer;
    /** Object hierarchy serializer for the serialization of uobject hierarchies */
    UObjectHierarchySerializer* ObjectHierarchySerializer;
    /** Additional data serialized by the asset type serializer */
    TSharedPtr<FJsonObject> AssetSerializedData;

    /** Internal constructor */
    FSerializationContext(const FString& RootOutputDirectory, const FAssetData& AssetData, UPackage* Package);

    /** Finalizes serialization by writing resulting JSON file containing object hierarchy and additional information */
    void Finalize() const;
public:
    ~FSerializationContext();

    /** The path to the package this asset is located in, with final package name stripped, like /Game/Path */
    FORCEINLINE FString GetPackagePath() const {
        return AssetData.PackagePath.ToString();
    }

    /** The name of the package in which the asset is found, this is the full long package name such as /Game/Path/Package */
    FORCEINLINE FString GetPackageName() const {
        return AssetData.PackageName.ToString();
    }

    /** Returns name of the asset inside of the package, e.g AssetName */
    FORCEINLINE FString GetAssetName() const {
        return AssetData.AssetName.ToString();
    }

    /** Returns asset data backing this asset object */
    FORCEINLINE const FAssetData& GetAssetData() const {
        return AssetData;
    }

    /** Return property serializer usable for serializing UProperty values */
    FORCEINLINE UPropertySerializer* GetPropertySerializer() const {
        return PropertySerializer;
    }

    /** Returns serializer capable of serializing uobject hierarchies */
    FORCEINLINE UObjectHierarchySerializer* GetObjectSerializer() const {
        return ObjectHierarchySerializer;
    }

    /** Returns json object used for writing additional asset-related information for the asset type serializer */
    FORCEINLINE TSharedRef<FJsonObject> GetData() const {
        return AssetSerializedData.ToSharedRef();
    }

    /** Returns package object containing provided asset */
    FORCEINLINE UPackage* GetPackage() const {
        return AssetData.GetPackage();
    }

    /** Retrieves asset object and makes sure it matches the specified type */
    template<typename T>
    T* GetAsset() const {
        // Make sure we are not trying to retrieve a blueprint, because it's a pretty common source of errors
        checkf(!T::StaticClass()->IsChildOf(UBlueprint::StaticClass()), TEXT("Cannot access 'Blueprint' asset object in cooked data as it has been stripped. Use GetBlueprintAsset instead."));

        // Retrieve asset object and make sure cast is successful
        T* AssetObject = FindObject<T>(Package, *AssetData.AssetName.ToString());
        checkf(AssetObject, TEXT("Failed to find Asset %s inside of the package %s"), *AssetData.AssetName.ToString(), *AssetData.PackageName.ToString());
        return AssetObject;
    }

    /** Retrieves blueprint generated class if current asset represents a blueprint. Use this instead of GetAsset for blueprint */
    template<typename T = UBlueprintGeneratedClass>
    T* GetBlueprintAsset() const {
        // Make sure argument we received actually makes sense
        check(T::StaticClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass()));

        // Retrieve GeneratedClass tag containing a text path to generated class object
        // All blueprints have it set, so if GetTagValue returns false, we are dealing with non-blueprint asset
        FString GeneratedClassExportedPath;
        if (!AssetData.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassExportedPath)) {
            checkf(0, TEXT("GetBlueprintAsset called on non-blueprint asset"));
            return NULL;
        }

        // Make sure export path represents a valid path and convert it to pure object path
        FString GeneratedClassPath;
        check(FPackageName::ParseExportTextPath(GeneratedClassExportedPath, NULL, &GeneratedClassPath));
        const FString BlueprintClassObjectName = FPackageName::ObjectPathToObjectName(GeneratedClassPath);

        // Load UBlueprintGeneratedClass for provided object and make sure it has been loaded
        T* ClassObject = FindObject<T>(Package, *BlueprintClassObjectName);
        checkf(ClassObject, TEXT("Failed to find Generated Class %s inside of the package %s"), *BlueprintClassObjectName, *AssetData.PackageName.ToString());
        return ClassObject;
    }

    /** Returns file path for the dump output file with provided postfix (can be empty) and extension. File is placed in the base asset directory */
    FORCEINLINE FString GetDumpFilePath(const FString& Postfix, const FString& Extension) const {
        FString Filename = FPackageName::GetShortName(GetPackageName());

        // TODO can we even have multiple assets in one package? what should be the treatment for that case?
        // We cannot really specify that information inside of the filename because we can lookup entire package by request,
        // and not just a single asset object, and iterating files to find exact json name is too expensive for massive dumping
        // As far as I'm aware, package can only contain one asset, because usually it only contains one top level object,
        // and only top level objects are considered to be assets (so f.e. font-embedded textures do not represent separate assets)
        // Filename.AppendChar('-').Append(GetAssetName());

        if (Postfix.Len() > 0) {
            Filename.AppendChar('-').Append(Postfix);
        }
        if (Extension.Len() > 0) {
            if (Extension[0] != '.') Filename.AppendChar('.');
            Filename.Append(Extension);
        }
        return FPaths::Combine(PackageBaseDirectory, Filename);
    }
};
