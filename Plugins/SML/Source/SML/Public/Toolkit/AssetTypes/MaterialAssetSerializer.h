#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "MaterialAssetSerializer.generated.h"

UCLASS()
class SML_API UMaterialAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    /*static void RegisterShaderInitRHIHook();*/
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    // TODO uncomment once we rework material serialization for UE4.25
    /*static void SerializeMaterialResource(class FMaterialResource* MaterialResource, TSharedPtr<FJsonObject> OutObject, UObjectHierarchySerializer* ObjectHierarchySerializer, FAssetSerializationContext& Context);
    static void SerializeShader(class FShader* Shader, TSharedPtr<FJsonObject> OutJson, FAssetSerializationContext& AssetSerializationContext, const FString& ShaderNamePrefix);
    static TSharedPtr<FJsonObject> SerializeParameterBindings(class FShaderParameterBindings* ParameterBindings);
    static TSharedPtr<FJsonObject> SerializeShaderParameterMapInfo(const class FShaderParameterMapInfo* ParameterMapInfo);
    static TSharedPtr<FJsonObject> SerializeMaterialShaderMapId(const class FMaterialShaderMapId& MaterialShaderMapId, UPropertySerializer* PropertySerializer);
    static void SerializeMaterialCompilationOutput(class FMaterialShaderMap* ShaderMap, TSharedPtr<FJsonObject> MaterialCompilationOutput);
    static TSharedPtr<FJsonObject> SerializeUniformExpression(class FMaterialUniformExpression* Expression);
    static void SerializeMaterialValue(uint32 MaterialValueType, const FLinearColor& MaterialValue, TSharedPtr<FJsonObject> OutJson);
    static TSharedPtr<FJsonObject> SerializeLinearColor(const FLinearColor& Color);
    static TSharedPtr<FJsonObject> SerializeParameterInfo(const struct FMaterialParameterInfo& ParameterInfo);
    static void SerializeCommonTextureExpression(FMaterialUniformExpression* Expression, TSharedPtr<FJsonObject> OutObject);
    static void SerializeCommonExternalTextureBaseExpression(FMaterialUniformExpression* Expression, TSharedPtr<FJsonObject> OutObject);
    static void UncompressShaderCode(const class FShaderResource& ShaderResource, TArray<uint8>& UncompressedCode);

    static FString MaterialValueTypeToString(uint32 MaterialValueType);
    static FString FoldedMathOperationToString(uint8 FoldedMathOperation);
    static FString TrigMathOperationToString(uint8 TrigMathOperation);
    static FString MaterialShaderMapUsageToString(uint8 MaterialShaderMapUsage);
    static FString ShaderPlatformToString(uint8 ShaderPlatform);
    static FString ShaderFrequencyToString(uint8 ShaderFrequency);*/

    virtual FName GetAssetClass() const override;
};
