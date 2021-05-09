#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetTypeSerializer.generated.h"

class FSerializationContext;

UCLASS(Abstract)
class SML_API UAssetTypeSerializer : public UObject {
    GENERATED_BODY()
public:
    /**
     * Performs asset serialization and writes result into the out json object
     * Object hierarchy serializer is already populated with fresh copy of property serializer and source asset package
     * Serialized object hierarchy will be automatically appended into output json object after function return
     * Type of asset package is automatically written into the output object type, additionally to package path
     */
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const PURE_VIRTUAL(UAssetTypeSerializer::SerializeAsset,);

    /**
     * Returns asset class tis handler is capable of serializing
     * Note that this method will be called on Class Default Object
     */
    virtual FName GetAssetClass() const PURE_VIRTUAL(UAssetTypeSerializer::GetAssetCategory, return NAME_None;);

    /**
     * Allows type serializer to handle extra asset classes additional to it's primary class returned by GetAssetClass
     * Mostly intended to be used by asset serializers handling base types as a primary classes,
     * so here they can opt in to handle children asset types additionally
     * Keep in mind that primary asset class serializers are preferred over additional ones when multiple are present
     */
    virtual void GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) {
    }

    /** Determines whenever this asset should be serialized by default */
    virtual bool ShouldSerializeByDefault() const { return false; }

    /** Determines whenever this serializer supports being run in parallel in worker threads. Override and return false if you depend on main thread state */
    virtual bool SupportsParallelDumping() const { return true; }

    /**
     * Returns serializer capable of serializing asset of specified class
     * or NULL if such serializer cannot be resolved.
     * Uses reflection internally to find matching serializer
     */
    static UAssetTypeSerializer* FindSerializerForAssetClass(FName AssetClass);

    static TArray<UAssetTypeSerializer*> GetAvailableAssetSerializers();
};
