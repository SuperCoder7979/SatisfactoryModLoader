#pragma once
#include "Toolkit/AssetTypes/MaterialAssetSerializer.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UMaterialAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UMaterial)
        // TODO we do not serialize shaders yet, but information exposed by normal object serialization should be enough for reasonable stubs
        // obviously they will be unable to show material in editor, but they can be used to reference it and even create new instances on top of it
        SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UMaterialAssetSerializer::GetAssetClass() const {
    return UMaterial::StaticClass()->GetFName();
}

// TODO uncomment once we port Material shader serialization to UE4.25
/*static bool GHasShaderInitBeenHooked = false;

void UMaterialAssetSerializer::RegisterShaderInitRHIHook() {
    GHasShaderInitBeenHooked = true;
    SUBSCRIBE_METHOD(FShaderMapResource::InitRHI, [](auto& Call, FShaderMapResourceCode* ShaderResource) {
        // Prevent InitRHI from emptying shader bytecode array because we still need it for dumping
        const TArray<uint8> CopiedByteCodeArray = ShaderResource->Code;
        check(CopiedByteCodeArray.Num());
        Call(ShaderResource);
        ShaderResource->Code = CopiedByteCodeArray;
    });
}

void UMaterialAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    const TArray<UObject*> RootObjects = FAssetHelper::GetRootPackageObjects(AssetPackage);
    check(RootObjects.Num() == 1);

    UMaterial* Material;
    check(RootObjects.FindItemByClass<UMaterial>(&Material));

    // Collect material resources
    TSet<FMaterialResource*> MaterialResources;
    for (int32 FeatureLevel = 0; FeatureLevel < ERHIFeatureLevel::Num; FeatureLevel++) {
        for (int32 QualityLevel = 0; QualityLevel < EMaterialQualityLevel::Num; QualityLevel++) {
            FMaterialResource* MaterialResource = Material->GetMaterialResource((ERHIFeatureLevel::Type) FeatureLevel, EMaterialQualityLevel::High);
            if (MaterialResource != NULL && MaterialResource->GetGameThreadShaderMap() != NULL) {
                MaterialResources.Add(MaterialResource);
            }
        }
    }
    check(MaterialResources.Num() > 0);

    // Serialize normal material properties
    ObjectHierarchySerializer->SetObjectMark(Material, TEXT("RootMaterialObject"));
    const TSharedPtr<FJsonObject> MaterialProperties = ObjectHierarchySerializer->SerializeObjectProperties(Material);
    OutObject->SetObjectField(TEXT("MaterialProperties"), MaterialProperties);

    // Dump material resources general information into json and shader bytecode into binary files
    TArray<TSharedPtr<FJsonValue>> MaterialResourcesArray;
    for (FMaterialResource* MaterialResource : MaterialResources) {
        const TSharedPtr<FJsonObject> ResourceObject = MakeShareable(new FJsonObject());
        SerializeMaterialResource(MaterialResource, ResourceObject, ObjectHierarchySerializer, Context);
        MaterialResourcesArray.Add(MakeShareable(new FJsonValueObject(ResourceObject)));
    }
    OutObject->SetArrayField(TEXT("MaterialResources"), MaterialResourcesArray);
}

void UMaterialAssetSerializer::UncompressShaderCode(const FShaderResource& ShaderResource, TArray<uint8>& UncompressedCode) {
    checkf(GHasShaderInitBeenHooked, TEXT("Cannot dump shader bytecode outside of the development mode (due to performance reasons)"));
    check(ShaderResource.Code.Num());

    const FName ShaderCompressionFormat = NAME_Zlib;
    if (ShaderResource.Code.Num() != ShaderResource.UncompressedCodeSize) {
        UncompressedCode.SetNum(ShaderResource.UncompressedCodeSize);
        auto bSucceed = FCompression::UncompressMemory(ShaderCompressionFormat,
            UncompressedCode.GetData(),
            ShaderResource.UncompressedCodeSize,
            ShaderResource.Code.GetData(),
            ShaderResource.Code.Num());
        check(bSucceed);
    } else {
        UncompressedCode = ShaderResource.Code;
    }
}

void UMaterialAssetSerializer::SerializeShader(FShader* Shader, TSharedPtr<FJsonObject> OutJson, FAssetSerializationContext& AssetSerializationContext, const FString& ShaderNamePrefix) {
    // Skip OutputHash - stripped during cooking
    // Skip MaterialShaderMapHash - stripped during cooking
    // Skip VFSourceHash - stripped during cooking
    // Skip SourceHash - stripped during cooking
    // Skip ShaderPipeline - already serialized in SerializeShaderMap
    // Skip VFType - serialized in SerializeMaterialResource
    // Skip serializing platform because it is already serialized

    // Serialize basic information
    const FString FrequencyString = ShaderFrequencyToString(Shader->Target.GetFrequency());
    const FString ShaderType = Shader->Type->GetName();

    OutJson->SetStringField(TEXT("ShaderType"), ShaderType);
    OutJson->SetNumberField(TEXT("PermutationId"), Shader->PermutationId);
    OutJson->SetStringField(TEXT("ShaderFrequency"), FrequencyString);
    OutJson->SetNumberField(TEXT("NumInstructions"), Shader->GetNumInstructions());

    // Serialize uniform buffer parameters
    // Uniform parameters describe offset of each buffer inside of the shader and
    // it's internal structure used by shader
    TArray<TSharedPtr<FJsonValue>> UniformBufferParameters;
    for (int32 i = 0; i < Shader->UniformBufferParameters.Num(); i++) {
        const TCHAR* StructTypeName = Shader->UniformBufferParameterStructs[i]->GetStructTypeName();
        FShaderUniformBufferParameter* Parameter = Shader->UniformBufferParameters[i];

        TSharedPtr<FJsonObject> UniformBufferParameter = MakeShareable(new FJsonObject());
        UniformBufferParameter->SetStringField(TEXT("StructTypeName"), StructTypeName);
        UniformBufferParameter->SetNumberField(TEXT("BaseIndex"), Parameter->GetBaseIndex());
        UniformBufferParameter->SetBoolField(TEXT("bIsBound"), Parameter->IsBound());
        UniformBufferParameters.Add(MakeShareable(new FJsonValueObject(UniformBufferParameter)));
    }
    OutJson->SetArrayField(TEXT("UniformBufferParameters"), UniformBufferParameters);

    // Serialize bindings
    // Bindings link parameters struct members (by offset) to shader parameters (by base index and buffer index)
    // Relevant method to see how it is done: SetShaderParameters
    // Apparently it is only populated through BindForLegacyShaderParameters method call,
    // which is performed only for global shaders using SHADER_USE_PARAMETER_STRUCT macro
    // with predefined struct layout. So yeah, it should be empty for non-global shaders at all times
    // adn to get C++ type of struct in question, you should look at shader type in question
    OutJson->SetObjectField(TEXT("Bindings"), SerializeParameterBindings(&Shader->Bindings));

    // Serialize shader parameter map info
    // Parameter map info contains indices of each parameter and describes layout of shader bindings in memory
    // Layout of fields in parameter map strictly follows layout of parameters inside of the shader RHI resource
    OutJson->SetObjectField(TEXT("ParameterMapInfo"), SerializeShaderParameterMapInfo(&Shader->GetParameterMapInfo()));

    // Serialize bytecode into separate file
    TArray<uint8> ShaderBytecode;
    UncompressShaderCode(*Shader->Resource, ShaderBytecode);

    const FString ShaderNamePostfix = FString::Printf(TEXT("_%s_%s_%s"), *ShaderNamePrefix, *ShaderType, *FrequencyString);
    AssetSerializationContext.SaveAdditionalAssetFile(TEXT("bin"), ShaderBytecode, *ShaderNamePostfix);
}

TSharedPtr<FJsonObject> UMaterialAssetSerializer::SerializeParameterBindings(FShaderParameterBindings* ParameterBindings) {
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

    TArray<TSharedPtr<FJsonValue>> Parameters;
    for (const FShaderParameterBindings::FParameter& Parameter : ParameterBindings->Parameters) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), Parameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("BufferIndex"), Parameter.BufferIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_Inside_ParameterStruct"), Parameter.ByteOffset);
        ParameterObject->SetNumberField(TEXT("ByteSize"), Parameter.ByteSize);
        Parameters.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("Parameters"), Parameters);

    TArray<TSharedPtr<FJsonValue>> Textures;
    for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ParameterBindings->Textures) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), ResourceParameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        Textures.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("Textures"), Textures);

    TArray<TSharedPtr<FJsonValue>> SRVs;
    for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ParameterBindings->SRVs) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), ResourceParameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        SRVs.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("SRVs"), SRVs);

    TArray<TSharedPtr<FJsonValue>> Samplers;
    for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ParameterBindings->Samplers) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), ResourceParameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        Samplers.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("Samplers"), Samplers);

    TArray<TSharedPtr<FJsonValue>> GraphTextures;
    for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ParameterBindings->GraphTextures) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), ResourceParameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        GraphTextures.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("GraphTextures"), GraphTextures);

    TArray<TSharedPtr<FJsonValue>> GraphSRVs;
    for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ParameterBindings->GraphSRVs) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), ResourceParameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        GraphSRVs.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("GraphSRVs"), GraphSRVs);

    TArray<TSharedPtr<FJsonValue>> GraphUAVs;
    for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ParameterBindings->GraphUAVs) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), ResourceParameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        GraphUAVs.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("GraphUAVs"), GraphUAVs);

    TArray<TSharedPtr<FJsonValue>> ParameterReferences;
    for (const FShaderParameterBindings::FParameterStructReference& ResourceParameter : ParameterBindings->ParameterReferences) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BufferIndex"), ResourceParameter.BufferIndex);
        ParameterObject->SetNumberField(TEXT("ByteOffset_InParametersStruct"), ResourceParameter.ByteOffset);
        ParameterReferences.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("ParameterReferences"), ParameterReferences);

    JsonObject->SetNumberField(TEXT("RootParameterBufferIndex"), ParameterBindings->RootParameterBufferIndex);
    return JsonObject;
}

TSharedPtr<FJsonObject> UMaterialAssetSerializer::SerializeShaderParameterMapInfo(const FShaderParameterMapInfo* ParameterMapInfo) {
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

    TArray<TSharedPtr<FJsonValue>> UniformBuffers;
    for (const FShaderParameterInfo& Parameter : ParameterMapInfo->UniformBuffers) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), Parameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("Size"), Parameter.Size);
        UniformBuffers.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("UniformBuffers"), UniformBuffers);

    TArray<TSharedPtr<FJsonValue>> TextureSamplers;
    for (const FShaderParameterInfo& Parameter : ParameterMapInfo->TextureSamplers) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), Parameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("Size"), Parameter.Size);
        TextureSamplers.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("TextureSamplers"), TextureSamplers);

    TArray<TSharedPtr<FJsonValue>> SRVs;
    for (const FShaderParameterInfo& Parameter : ParameterMapInfo->SRVs) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BaseIndex"), Parameter.BaseIndex);
        ParameterObject->SetNumberField(TEXT("Size"), Parameter.Size);
        SRVs.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("SRVs"), SRVs);

    TArray<TSharedPtr<FJsonValue>> LooseParameterBuffers;
    for (const FShaderLooseParameterBufferInfo& Parameter : ParameterMapInfo->LooseParameterBuffers) {
        TSharedPtr<FJsonObject> ParameterObject = MakeShareable(new FJsonObject());
        ParameterObject->SetNumberField(TEXT("BufferIndex"), Parameter.BufferIndex);
        ParameterObject->SetNumberField(TEXT("BufferSize"), Parameter.BufferSize);
        TArray<TSharedPtr<FJsonValue>> Parameters;
        for (const FShaderParameterInfo& ParameterInfo : Parameter.Parameters) {
            TSharedPtr<FJsonObject> ParameterInnerObject = MakeShareable(new FJsonObject());
            ParameterInnerObject->SetNumberField(TEXT("BaseIndex"), ParameterInfo.BaseIndex);
            ParameterInnerObject->SetNumberField(TEXT("Size"), ParameterInfo.Size);
            Parameters.Add(MakeShareable(new FJsonValueObject(ParameterInnerObject)));
        }
        ParameterObject->SetArrayField(TEXT("Parameters"), Parameters);
        LooseParameterBuffers.Add(MakeShareable(new FJsonValueObject(ParameterObject)));
    }
    JsonObject->SetArrayField(TEXT("LooseParameterBuffers"), LooseParameterBuffers);

    return JsonObject;
}

template<typename TShaderType>
void SerializeShaderMap(TShaderMap<TShaderType>* ShaderMap, TSharedPtr<FJsonObject> OutJson, FAssetSerializationContext& Context, const FString& ShaderNamePrefix) {
    // Serialize shader platform before serializing any shaders
    const FString ShaderPlatform = UMaterialAssetSerializer::ShaderPlatformToString(ShaderMap->GetShaderPlatform());
    OutJson->SetStringField(TEXT("ShaderPlatform"), ShaderPlatform);
    const FString ShaderNamePrefixWithPlatform = FString::Printf(TEXT("%s_%s"), *ShaderNamePrefix, *ShaderPlatform);

    TArray<TSharedPtr<FJsonValue>> Shaders;
    for (const TPair<TShaderTypePermutation<FShaderType>, TRefCountPtr<FShader>>& Pair : ShaderMap->GetShaders()) {

        const TSharedPtr<FJsonObject> OutObject = MakeShareable(new FJsonObject());
        const FString IndependentShaderName = FString::Printf(TEXT("%s_root"), *ShaderNamePrefixWithPlatform);
        UMaterialAssetSerializer::SerializeShader(Pair.Value, OutObject, Context, ShaderNamePrefix);

        Shaders.Add(MakeShareable(new FJsonValueObject(OutObject)));
    }

    OutJson->SetArrayField(TEXT("Shaders"), Shaders);

    TArray<FShaderPipeline*> OutShaderPipelines;
    ShaderMap->GetShaderPipelineList(OutShaderPipelines, FShaderPipeline::EFilter::EAll);

    TArray<TSharedPtr<FJsonValue>> ShaderPipelines;
    for (FShaderPipeline* ShaderPipeline : OutShaderPipelines) {
        TSharedPtr<FJsonObject> OutObject = MakeShareable(new FJsonObject());
        const FString PipelineTypeName = ShaderPipeline->PipelineType->GetName();
        OutObject->SetStringField(TEXT("PipelineType"), PipelineTypeName);
        TArray<TSharedPtr<FJsonValue>> PipelineShaders;
        for (FShader* PipelineShader : ShaderPipeline->GetShaders()) {

            const TSharedPtr<FJsonObject> ShaderObject = MakeShareable(new FJsonObject());
            const FString PipelineShaderName = FString::Printf(TEXT("%s_%s"), *ShaderNamePrefixWithPlatform, *PipelineTypeName);
            UMaterialAssetSerializer::SerializeShader(PipelineShader, ShaderObject, Context, PipelineShaderName);

            PipelineShaders.Add(MakeShareable(new FJsonValueObject(ShaderObject)));
        }
        OutObject->SetArrayField(TEXT("PipelineShaders"), PipelineShaders);
        ShaderPipelines.Add(MakeShareable(new FJsonValueObject(OutObject)));
    }

    OutJson->SetArrayField(TEXT("ShaderPipelines"), ShaderPipelines);
}

static FString QualityLevelToString(uint8 QualityLevel) {
    switch(QualityLevel) {
       case EMaterialQualityLevel::High: return TEXT("High");
       case EMaterialQualityLevel::Medium: return TEXT("Medium");
       case EMaterialQualityLevel::Low: return TEXT("Low");
       default: checkf(0, TEXT("Unknown quality level: %d"), QualityLevel); return TEXT("");
    }
}

static FString FeatureLevelToString(uint8 FeatureLevel) {
    switch (FeatureLevel) {
        case ERHIFeatureLevel::ES3_1: return TEXT("ES3_1");
        case ERHIFeatureLevel::SM5: return TEXT("SM5");
        default: checkf(0, TEXT("Unknown feature level: %d"), FeatureLevel); return TEXT("");
    }
}

void UMaterialAssetSerializer::SerializeMaterialResource(FMaterialResource* MaterialResource, TSharedPtr<FJsonObject> OutObject, UObjectHierarchySerializer* ObjectHierarchySerializer, FAssetSerializationContext& Context) {
    UPropertySerializer* PropertySerializer = ObjectHierarchySerializer->GetPropertySerializer<UPropertySerializer>();

    const FString QualityLevelString = QualityLevelToString(MaterialResource->GetQualityLevel());
    const FString FeatureLevelString = FeatureLevelToString(MaterialResource->GetFeatureLevel());

    // Serialize basic attributes of material resource
    OutObject->SetStringField(TEXT("QualityLevel"), QualityLevelString);
    OutObject->SetStringField(TEXT("FeatureLevel"), FeatureLevelString);

    FMaterialShaderMap* ShaderMap = MaterialResource->GetGameThreadShaderMap();
    check(ShaderMap);
    const FMaterialShaderMapId& MaterialShaderMapId = ShaderMap->GetShaderMapId();

    // Serialize material shader map identifier
    OutObject->SetObjectField(TEXT("MaterialShaderMapId"), SerializeMaterialShaderMapId(MaterialShaderMapId, PropertySerializer));

    // Serialize MaterialCompilationOutput fields
    const TSharedPtr<FJsonObject> MaterialCompilationOutput = MakeShareable(new FJsonObject());
    SerializeMaterialCompilationOutput(ShaderMap, MaterialCompilationOutput);
    OutObject->SetObjectField(TEXT("MaterialCompilationOutput"), MaterialCompilationOutput);

    // Serialize actual shader map contents
    const FString MainShadersPrefix = FString::Printf(TEXT("main_%s_%s"), *QualityLevelString, *FeatureLevelString);
    SerializeShaderMap<FMaterialShaderType>(ShaderMap, OutObject, Context, MainShadersPrefix);

    // Serialize child mesh-dependent shader maps
    TArray<TSharedPtr<FJsonValue>> MeshShaderMaps;
    for (FMeshMaterialShaderMap& MeshMaterialShaderMap : ShaderMap->MeshShaderMaps) {
        TSharedPtr<FJsonObject> MapObject = MakeShareable(new FJsonObject());
        const FString VertexFactoryName = MeshMaterialShaderMap.GetVertexFactoryType()->GetName();
        MapObject->SetStringField(TEXT("VertexFactoryType"), VertexFactoryName);

        const FString VertexTypeShadersPrefix = FString::Printf(TEXT("%s_%s_%s"), *VertexFactoryName, *QualityLevelString, *FeatureLevelString);
        SerializeShaderMap<FMeshMaterialShaderType>(&MeshMaterialShaderMap, MapObject, Context, VertexTypeShadersPrefix);
        MeshShaderMaps.Add(MakeShareable(new FJsonValueObject(MapObject)));
    }
    OutObject->SetArrayField(TEXT("MeshShaderMaps"), MeshShaderMaps);
}

TSharedPtr<FJsonObject> UMaterialAssetSerializer::SerializeMaterialShaderMapId(const FMaterialShaderMapId& MaterialShaderMapId, UPropertySerializer* PropertySerializer) {
    TSharedPtr<FJsonObject> OutObject = MakeShareable(new FJsonObject());

    // Serialize MaterialShaderMap identifier with all child fields
    OutObject->SetStringField(TEXT("BaseMaterialId"), MaterialShaderMapId.BaseMaterialId.ToString());
    OutObject->SetStringField(TEXT("Usage"), MaterialShaderMapUsageToString(MaterialShaderMapId.Usage));
    OutObject->SetObjectField(TEXT("ParameterSet"), PropertySerializer->SerializeStruct(FStaticParameterSet::StaticStruct(), &MaterialShaderMapId.GetParameterSet()));
    OutObject->SetStringField(TEXT("ParameterSetLayerParametersKey"), MaterialShaderMapId.GetParameterSetLayerParametersKey());

    // Serialize list of referenced functions and parameters collections
    TArray<TSharedPtr<FJsonValue>> ReferencedFunctions;
    for (const FGuid& FunctionStateId : MaterialShaderMapId.ReferencedFunctions) {
        ReferencedFunctions.Add(MakeShareable(new FJsonValueString(FunctionStateId.ToString())));
    }
    OutObject->SetArrayField(TEXT("ReferencedFunctions"), ReferencedFunctions);

    TArray<TSharedPtr<FJsonValue>> ReferencedParameterCollections;
    for (const FGuid& ParameterCollectionId: MaterialShaderMapId.ReferencedParameterCollections) {
        ReferencedParameterCollections.Add(MakeShareable(new FJsonValueString(ParameterCollectionId.ToString())));
    }
    OutObject->SetArrayField(TEXT("ReferencedParameterCollections"), ReferencedParameterCollections);

    // Serialize dependencies on other shaders
    TArray<TSharedPtr<FJsonValue>> ShaderTypeDependencies;
    for (const FShaderTypeDependency& Dependency : MaterialShaderMapId.ShaderTypeDependencies) {
        TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
        JsonObject->SetStringField(TEXT("ShaderType"), Dependency.ShaderType->GetName());
        JsonObject->SetNumberField(TEXT("PermutationId"), Dependency.PermutationId);
        // Skip SourceHash because it will be empty in shipping binaries
        ShaderTypeDependencies.Add(MakeShareable(new FJsonValueObject(JsonObject)));
    }
    OutObject->SetArrayField(TEXT("ShaderTypeDependencies"), ShaderTypeDependencies);

    TArray<TSharedPtr<FJsonValue>> PipelineTypeDependencies;
    for (const FShaderPipelineTypeDependency& Dependency : MaterialShaderMapId.ShaderPipelineTypeDependencies) {
        const FString PipelineType = Dependency.ShaderPipelineType->GetName();
        PipelineTypeDependencies.Add(MakeShareable(new FJsonValueString(PipelineType)));
    }
    OutObject->SetArrayField(TEXT("ShaderPipelineTypeDependencies"), PipelineTypeDependencies);

    TArray<TSharedPtr<FJsonValue>> VertexFactoryTypeDependencies;
    for (const FVertexFactoryTypeDependency& FactoryTypeDependency : MaterialShaderMapId.VertexFactoryTypeDependencies) {
        const FString VertexFactoryType = FactoryTypeDependency.VertexFactoryType->GetName();
        VertexFactoryTypeDependencies.Add(MakeShareable(new FJsonValueString(VertexFactoryType)));
    }
    OutObject->SetArrayField(TEXT("VertexFactoryTypeDependencies"), VertexFactoryTypeDependencies);
    return OutObject;
}

void UMaterialAssetSerializer::SerializeMaterialCompilationOutput(FMaterialShaderMap* ShaderMap, TSharedPtr<FJsonObject> MaterialCompilationOutput) {

    // Serialize normal compiler output properties
    MaterialCompilationOutput->SetBoolField(TEXT("RequiresSceneColorCopy"), ShaderMap->RequiresSceneColorCopy());
    MaterialCompilationOutput->SetBoolField(TEXT("NeedsSceneTextures"), ShaderMap->NeedsSceneTextures());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesGlobalDistanceField"), ShaderMap->UsesGlobalDistanceField());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesWorldPositionOffset"), ShaderMap->UsesWorldPositionOffset());
    MaterialCompilationOutput->SetBoolField(TEXT("NeedsGBuffer"), ShaderMap->NeedsGBuffer());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesEyeAdaptation"), ShaderMap->UsesEyeAdaptation());
    MaterialCompilationOutput->SetBoolField(TEXT("ModifiesMeshPosition"), ShaderMap->ModifiesMeshPosition());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesPixelDepthOffset"), ShaderMap->UsesPixelDepthOffset());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesSceneDepthLookup"), ShaderMap->UsesSceneDepthLookup());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesVelocitySceneTexture"), ShaderMap->UsesVelocitySceneTexture());
    MaterialCompilationOutput->SetBoolField(TEXT("UsesDistanceCullFade"), ShaderMap->UsesDistanceCullFade());
    MaterialCompilationOutput->SetNumberField(TEXT("NumUsedUVScalars"), ShaderMap->GetNumUsedUVScalars());
    MaterialCompilationOutput->SetNumberField(TEXT("NumUsedCustomInterpolatorScalars"), ShaderMap->GetNumUsedCustomInterpolatorScalars());

    uint32 EstimatedNumTextureSamplesVS;
    uint32 EstimatedNumTextureSamplesPS;
    ShaderMap->GetEstimatedNumTextureSamples(EstimatedNumTextureSamplesVS, EstimatedNumTextureSamplesPS);

    MaterialCompilationOutput->SetNumberField(TEXT("EstimatedNumTextureSamplesVS"), EstimatedNumTextureSamplesVS);
    MaterialCompilationOutput->SetNumberField(TEXT("EstimatedNumTextureSamplesPS"), EstimatedNumTextureSamplesPS);

    const FUniformExpressionSet& UniformExpressionSet = ShaderMap->GetUniformExpressionSet();

    // Serialize uniforms
    TArray<TSharedPtr<FJsonValue>> UniformVectorExpressions;
    for (const TRefCountPtr<FMaterialUniformExpression>& Expression : UniformExpressionSet.UniformVectorExpressions) {
        const TSharedPtr<FJsonObject> ExpressionObject = SerializeUniformExpression(Expression);
        UniformVectorExpressions.Add(MakeShareable(new FJsonValueObject(ExpressionObject)));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("UniformVectorExpressions"), UniformVectorExpressions);

    TArray<TSharedPtr<FJsonValue>> UniformScalarExpressions;
    for (const TRefCountPtr<FMaterialUniformExpression>& Expression : UniformExpressionSet.UniformScalarExpressions) {
        const TSharedPtr<FJsonObject> ExpressionObject = SerializeUniformExpression(Expression);
        UniformScalarExpressions.Add(MakeShareable(new FJsonValueObject(ExpressionObject)));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("UniformScalarExpressions"), UniformScalarExpressions);

    TArray<TSharedPtr<FJsonValue>> UniformTexture2DExpressions;
    for (const TRefCountPtr<FMaterialUniformExpressionTexture>& Expression : UniformExpressionSet.Uniform2DTextureExpressions) {
        const TSharedPtr<FJsonObject> ExpressionObject = SerializeUniformExpression(Expression);
        UniformTexture2DExpressions.Add(MakeShareable(new FJsonValueObject(ExpressionObject)));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("UniformTexture2DExpressions"), UniformTexture2DExpressions);

    TArray<TSharedPtr<FJsonValue>> UniformTextureCubeExpressions;
    for (const TRefCountPtr<FMaterialUniformExpressionTexture>& Expression : UniformExpressionSet.UniformCubeTextureExpressions) {
        const TSharedPtr<FJsonObject> ExpressionObject = SerializeUniformExpression(Expression);
        UniformTextureCubeExpressions.Add(MakeShareable(new FJsonValueObject(ExpressionObject)));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("UniformTextureCubeExpressions"), UniformTextureCubeExpressions);

    TArray<TSharedPtr<FJsonValue>> UniformTextureVolumeExpressions;
    for (const TRefCountPtr<FMaterialUniformExpressionTexture>& Expression : UniformExpressionSet.UniformVolumeTextureExpressions) {
        const TSharedPtr<FJsonObject> ExpressionObject = SerializeUniformExpression(Expression);
        UniformTextureVolumeExpressions.Add(MakeShareable(new FJsonValueObject(ExpressionObject)));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("UniformTextureVolumeExpressions"), UniformTextureVolumeExpressions);

    TArray<TSharedPtr<FJsonValue>> UniformExternalTextureExpressions;
    for (const TRefCountPtr<FMaterialUniformExpressionExternalTexture>& Expression : UniformExpressionSet.UniformExternalTextureExpressions) {
        const TSharedPtr<FJsonObject> ExpressionObject = SerializeUniformExpression(Expression);
        UniformExternalTextureExpressions.Add(MakeShareable(new FJsonValueObject(ExpressionObject)));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("UniformExternalTextureExpressions"), UniformExternalTextureExpressions);

    // Serialize StateIds of used parameter collections
    TArray<TSharedPtr<FJsonValue>> ParameterCollections;
    for (const FGuid& ParameterCollectionGuid : UniformExpressionSet.ParameterCollections) {
        ParameterCollections.Add(MakeShareable(new FJsonValueString(ParameterCollectionGuid.ToString())));
    }
    MaterialCompilationOutput->SetArrayField(TEXT("ParameterCollections"), ParameterCollections);
}

static FString SamplerSourceModeToString(uint8 SamplerSourceMode) {
    switch (SamplerSourceMode) {
        case ESamplerSourceMode::SSM_FromTextureAsset: return TEXT("SSM_FromTextureAsset");
        case ESamplerSourceMode::SSM_Clamp_WorldGroupSettings: return TEXT("SSM_Clamp_WorldGroupSettings");
        case ESamplerSourceMode::SSM_Wrap_WorldGroupSettings: return TEXT("SSM_Wrap_WorldGroupSettings");
        default: checkf(0, TEXT("Unknown SamplerSourceMode: %d"), SamplerSourceMode); return TEXT("");
    }
}

void UMaterialAssetSerializer::SerializeCommonTextureExpression(FMaterialUniformExpression* Expression, TSharedPtr<FJsonObject> OutObject) {
    FMaterialUniformExpressionTexture* Texture = static_cast<FMaterialUniformExpressionTexture*>(Expression);
    OutObject->SetNumberField(TEXT("TextureIndex_In_ReferencedTextures"), Texture->TextureIndex);
    OutObject->SetStringField(TEXT("SamplerSource"),SamplerSourceModeToString(Texture->SamplerSource));
}

void UMaterialAssetSerializer::SerializeCommonExternalTextureBaseExpression(FMaterialUniformExpression* Expression, TSharedPtr<FJsonObject> OutObject) {
    FMaterialUniformExpressionExternalTextureBase* TextureBase = static_cast<FMaterialUniformExpressionExternalTextureBase*>(Expression);
    OutObject->SetNumberField(TEXT("SourceTextureIndex_In_ReferencedTextures"), TextureBase->SourceTextureIndex);
    OutObject->SetStringField(TEXT("ExternalTextureGuid_CompileTime"), TextureBase->ExternalTextureGuid.ToString());
}

static FString MaterialExposedTexturePropertyToString(uint8 TextureProperty) {
    switch (TextureProperty) {
        case EMaterialExposedTextureProperty::TMTM_TexelSize: return TEXT("TMTM_TexelSize");
        case EMaterialExposedTextureProperty::TMTM_TextureSize: return TEXT("TMTM_TextureSize");
        default: checkf(0, TEXT("Unknown MaterialExposedTextureProperty: %d"), TextureProperty); return TEXT("");
    }
}

TSharedPtr<FJsonObject> UMaterialAssetSerializer::SerializeUniformExpression(FMaterialUniformExpression* Expression) {
    TSharedPtr<FJsonObject> OutObject = MakeShareable(new FJsonObject());
    const FString ExpressionType = Expression->GetType()->GetName();
    OutObject->SetStringField(TEXT("ExpressionType"), ExpressionType);

    // BEGIN PARAMETERS AND CONSTANTS
    if (ExpressionType == TEXT("FMaterialUniformExpressionTexture")) {
        SerializeCommonTextureExpression(Expression, OutObject);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionConstant")) {
        FMaterialUniformExpressionConstant* Constant = static_cast<FMaterialUniformExpressionConstant*>(Expression);
        SerializeMaterialValue(Constant->ValueType, Constant->Value, OutObject);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionVectorParameter")) {
        FMaterialUniformExpressionVectorParameter* VectorParameter = static_cast<FMaterialUniformExpressionVectorParameter*>(Expression);
        OutObject->SetObjectField(TEXT("ParameterInfo"), SerializeParameterInfo(VectorParameter->ParameterInfo));
        OutObject->SetObjectField(TEXT("DefaultValue"), SerializeLinearColor(VectorParameter->DefaultValue));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionScalarParameter")) {
        FMaterialUniformExpressionScalarParameter* ScalarParameter = static_cast<FMaterialUniformExpressionScalarParameter*>(Expression);
        OutObject->SetObjectField(TEXT("ParameterInfo"), SerializeParameterInfo(ScalarParameter->ParameterInfo));
        OutObject->SetNumberField(TEXT("DefaultValue"), ScalarParameter->DefaultValue);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionTextureParameter")) {
        FMaterialUniformExpressionTextureParameter* TextureParameter = static_cast<FMaterialUniformExpressionTextureParameter*>(Expression);
        SerializeCommonTextureExpression(TextureParameter, OutObject);
        OutObject->SetObjectField(TEXT("ParameterInfo"), SerializeParameterInfo(TextureParameter->ParameterInfo));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionExternalTextureBase")) {
        SerializeCommonExternalTextureBaseExpression(Expression, OutObject);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionExternalTexture")) {
        FMaterialUniformExpressionExternalTexture* ExternalTexture = static_cast<FMaterialUniformExpressionExternalTexture*>(Expression);
        SerializeCommonExternalTextureBaseExpression(ExternalTexture, OutObject);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionExternalTextureParameter")) {
        FMaterialUniformExpressionExternalTextureParameter* TextureParameter = static_cast<FMaterialUniformExpressionExternalTextureParameter*>(Expression);
        SerializeCommonExternalTextureBaseExpression(TextureParameter, OutObject);
        OutObject->SetStringField(TEXT("ParameterName"), TextureParameter->ParameterName.ToString());

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionExternalTextureCoordinateScaleRotation")) {
        FMaterialUniformExpressionExternalTextureCoordinateScaleRotation* TextureCoordinateScaleRotation = static_cast<FMaterialUniformExpressionExternalTextureCoordinateScaleRotation*>(Expression);
        SerializeCommonExternalTextureBaseExpression(TextureCoordinateScaleRotation, OutObject);
        OutObject->SetBoolField(TEXT("HasParameterName"), TextureCoordinateScaleRotation->ParameterName.IsSet());
        OutObject->SetStringField(TEXT("ParameterName"), TextureCoordinateScaleRotation->ParameterName.Get(TEXT("<not specified>")).ToString());

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionExternalTextureCoordinateOffset")) {
        FMaterialUniformExpressionExternalTextureCoordinateOffset* TextureCoordinateOffset = static_cast<FMaterialUniformExpressionExternalTextureCoordinateOffset*>(Expression);
        SerializeCommonExternalTextureBaseExpression(TextureCoordinateOffset, OutObject);
        OutObject->SetBoolField(TEXT("HasParameterName"), TextureCoordinateOffset->ParameterName.IsSet());
        OutObject->SetStringField(TEXT("ParameterName"), TextureCoordinateOffset->ParameterName.Get(TEXT("<not specified>")).ToString());

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionFlipBookTextureParameter")) {
        FMaterialUniformExpressionFlipBookTextureParameter* FlipBookTextureParameter = static_cast<FMaterialUniformExpressionFlipBookTextureParameter*>(Expression);
        SerializeCommonTextureExpression(FlipBookTextureParameter, OutObject);

    }
    // BEGIN STANDARD MATH FUNCTIONS
    else if (ExpressionType == TEXT("FMaterialUniformExpressionSine")) {
        FMaterialUniformExpressionSine* Sine = static_cast<FMaterialUniformExpressionSine*>(Expression);
        OutObject->SetBoolField(TEXT("bIsCosine"), Sine->bIsCosine);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Sine->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionSquareRoot")) {
        FMaterialUniformExpressionSquareRoot* SquareRoot = static_cast<FMaterialUniformExpressionSquareRoot*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(SquareRoot->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionLength")) {
        FMaterialUniformExpressionLength* Length = static_cast<FMaterialUniformExpressionLength*>(Expression);
        OutObject->SetStringField(TEXT("ValueType"), MaterialValueTypeToString(Length->ValueType));
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Length->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionLogarithm2")) {
        FMaterialUniformExpressionLogarithm2* Logarithm2 = static_cast<FMaterialUniformExpressionLogarithm2*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Logarithm2->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionLogarithm10")) {
        FMaterialUniformExpressionLogarithm10* Logarithm10 = static_cast<FMaterialUniformExpressionLogarithm10*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Logarithm10->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionFoldedMath")) {
        FMaterialUniformExpressionFoldedMath* FoldedMath = static_cast<FMaterialUniformExpressionFoldedMath*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionA"), SerializeUniformExpression(FoldedMath->A));
        OutObject->SetObjectField(TEXT("ExpressionB"), SerializeUniformExpression(FoldedMath->B));
        OutObject->SetStringField(TEXT("ValueType"), MaterialValueTypeToString(FoldedMath->ValueType));
        OutObject->SetStringField(TEXT("Operation"), FoldedMathOperationToString(FoldedMath->Op));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionPeriodic")) {
        FMaterialUniformExpressionPeriodic* Periodic = static_cast<FMaterialUniformExpressionPeriodic*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Periodic->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionAppendVector")) {
        FMaterialUniformExpressionAppendVector* AppendVector = static_cast<FMaterialUniformExpressionAppendVector*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionA"), SerializeUniformExpression(AppendVector->A));
        OutObject->SetObjectField(TEXT("ExpressionB"), SerializeUniformExpression(AppendVector->B));
        OutObject->SetNumberField(TEXT("NumComponentsA"), AppendVector->NumComponentsA);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionMin")) {
        FMaterialUniformExpressionMin* Min = static_cast<FMaterialUniformExpressionMin*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionA"), SerializeUniformExpression(Min->A));
        OutObject->SetObjectField(TEXT("ExpressionB"), SerializeUniformExpression(Min->B));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionMax")) {
        FMaterialUniformExpressionMax* Max = static_cast<FMaterialUniformExpressionMax*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionA"), SerializeUniformExpression(Max->A));
        OutObject->SetObjectField(TEXT("ExpressionB"), SerializeUniformExpression(Max->B));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionClamp")) {
        FMaterialUniformExpressionClamp* Clamp = static_cast<FMaterialUniformExpressionClamp*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionInput"), SerializeUniformExpression(Clamp->Input));
        OutObject->SetObjectField(TEXT("ExpressionMin"), SerializeUniformExpression(Clamp->Min));
        OutObject->SetObjectField(TEXT("ExpressionMax"), SerializeUniformExpression(Clamp->Max));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionSaturate")) {
        FMaterialUniformExpressionSaturate* Saturate = static_cast<FMaterialUniformExpressionSaturate*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionInput"), SerializeUniformExpression(Saturate->Input));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionComponentSwizzle")) {
        FMaterialUniformExpressionComponentSwizzle* ComponentSwizzle = static_cast<FMaterialUniformExpressionComponentSwizzle*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(ComponentSwizzle->X));
        OutObject->SetNumberField(TEXT("IndexR"), ComponentSwizzle->IndexR);
        OutObject->SetNumberField(TEXT("IndexG"), ComponentSwizzle->IndexG);
        OutObject->SetNumberField(TEXT("IndexB"), ComponentSwizzle->IndexB);
        OutObject->SetNumberField(TEXT("IndexA"), ComponentSwizzle->IndexA);
        OutObject->SetNumberField(TEXT("NumElements"), ComponentSwizzle->NumElements);

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionFloor")) {
        FMaterialUniformExpressionFloor* Floor = static_cast<FMaterialUniformExpressionFloor*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Floor->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionCeil")) {
        FMaterialUniformExpressionCeil* Ceil = static_cast<FMaterialUniformExpressionCeil*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Ceil->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionFrac")) {
        FMaterialUniformExpressionFrac* Frac = static_cast<FMaterialUniformExpressionFrac*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Frac->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionFmod")) {
        FMaterialUniformExpressionFmod* Mod = static_cast<FMaterialUniformExpressionFmod*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionA"), SerializeUniformExpression(Mod->A));
        OutObject->SetObjectField(TEXT("ExpressionB"), SerializeUniformExpression(Mod->B));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionAbs")) {
        FMaterialUniformExpressionAbs* Abs = static_cast<FMaterialUniformExpressionAbs*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Abs->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionTextureProperty")) {
        FMaterialUniformExpressionTextureProperty* TextureProperty = static_cast<FMaterialUniformExpressionTextureProperty*>(Expression);
        OutObject->SetObjectField(TEXT("TextureExpression"), SerializeUniformExpression(TextureProperty->TextureExpression));
        OutObject->SetStringField(TEXT("TextureProperty"), MaterialExposedTexturePropertyToString(TextureProperty->TextureProperty));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionTrigMath")) {
        FMaterialUniformExpressionTrigMath* TrigMath = static_cast<FMaterialUniformExpressionTrigMath*>(Expression);
        OutObject->SetObjectField(TEXT("ExpressionA"), SerializeUniformExpression(TrigMath->X));
        OutObject->SetObjectField(TEXT("ExpressionB"), SerializeUniformExpression(TrigMath->Y));
        OutObject->SetStringField(TEXT("Operation"), TrigMathOperationToString(TrigMath->Op));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionRound")) {
        FMaterialUniformExpressionRound* Round = static_cast<FMaterialUniformExpressionRound*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Round->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionTruncate")) {
        FMaterialUniformExpressionTruncate* Truncate = static_cast<FMaterialUniformExpressionTruncate*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Truncate->X));

    } else if (ExpressionType == TEXT("FMaterialUniformExpressionSign")) {
        FMaterialUniformExpressionSign* Sign = static_cast<FMaterialUniformExpressionSign*>(Expression);
        OutObject->SetObjectField(TEXT("Expression"), SerializeUniformExpression(Sign->X));

    } else {
        checkf(0, TEXT("Unsupported uniform expression type: %s"), *ExpressionType);
    }
    return OutObject;
}

FString UMaterialAssetSerializer::MaterialValueTypeToString(uint32 MaterialValueType) {
    FString OutMaterialType;
    if (MaterialValueType == EMaterialValueType::MCT_Float1)
        OutMaterialType = TEXT("MCT_Float1");
    else if (MaterialValueType == EMaterialValueType::MCT_Float2)
        OutMaterialType = TEXT("MCT_Float2");
    else if (MaterialValueType == EMaterialValueType::MCT_Float3)
        OutMaterialType = TEXT("MCT_Float3");
    else if (MaterialValueType == EMaterialValueType::MCT_Float4)
        OutMaterialType = TEXT("MCT_Float4");
    else if (MaterialValueType == EMaterialValueType::MCT_Float)
        OutMaterialType = TEXT("MCT_Float");
    else if (MaterialValueType == EMaterialValueType::MCT_Texture2D)
        OutMaterialType = TEXT("MCT_Texture");
    else if (MaterialValueType == EMaterialValueType::MCT_TextureCube)
        OutMaterialType = TEXT("MCT_TextureCube");
    else if (MaterialValueType == EMaterialValueType::MCT_VolumeTexture)
        OutMaterialType = TEXT("MCT_VolumeTexture");
    else if (MaterialValueType == EMaterialValueType::MCT_StaticBool)
        OutMaterialType = TEXT("MCT_StaticBool");
    else if (MaterialValueType == EMaterialValueType::MCT_Unknown)
        OutMaterialType = TEXT("MCT_Unknown");
    else if (MaterialValueType == EMaterialValueType::MCT_MaterialAttributes)
        OutMaterialType = TEXT("MCT_MaterialAttributes");
    else if (MaterialValueType == EMaterialValueType::MCT_TextureExternal)
        OutMaterialType = TEXT("MCT_TextureExternal");
    else if (MaterialValueType == EMaterialValueType::MCT_Texture)
        OutMaterialType = TEXT("MCT_Texture");
    return OutMaterialType;
}

FString UMaterialAssetSerializer::FoldedMathOperationToString(uint8 FoldedMathOperation) {
    switch (FoldedMathOperation) {
        case EFoldedMathOperation::FMO_Add: return TEXT("FMO_Add");
        case EFoldedMathOperation::FMO_Cross: return TEXT("FMO_Cross");
        case EFoldedMathOperation::FMO_Div: return TEXT("FMO_Div");
        case EFoldedMathOperation::FMO_Dot: return TEXT("FMO_Div");
        case EFoldedMathOperation::FMO_Mul: return TEXT("FMO_Mul");
        case EFoldedMathOperation::FMO_Sub: return TEXT("FMO_Sub");
        default: checkf(0, TEXT("FoldedMathOperation unknown: %d"), FoldedMathOperation); return TEXT("");
    }
}

FString UMaterialAssetSerializer::TrigMathOperationToString(uint8 TrigMathOperation) {
    switch (TrigMathOperation) {
        case ETrigMathOperation::TMO_Acos: return TEXT("TMO_Acos");
        case ETrigMathOperation::TMO_Asin: return TEXT("TMO_Asin");
        case ETrigMathOperation::TMO_Atan: return TEXT("TMO_Atan");
        case ETrigMathOperation::TMO_Atan2: return TEXT("TMP_Atan2");
        case ETrigMathOperation::TMO_Cos: return TEXT("TMO_Cos");
        case ETrigMathOperation::TMO_Sin: return TEXT("TMO_Sin");
        case ETrigMathOperation::TMO_Tan: return TEXT("TMO_Tan");
        default: checkf(0, TEXT("TrigMathOperation unknown: %d"), TrigMathOperation); return TEXT("");
    }
}

FString UMaterialAssetSerializer::MaterialShaderMapUsageToString(uint8 MaterialShaderMapUsage) {
    switch (MaterialShaderMapUsage) {
        case EMaterialShaderMapUsage::Default: return TEXT("Default");
        case EMaterialShaderMapUsage::DebugViewMode: return TEXT("DebugViewMode");
        case EMaterialShaderMapUsage::LightmassExportDiffuse: return TEXT("LightmassExportDiffuse");
        case EMaterialShaderMapUsage::LightmassExportEmissive: return TEXT("LightmapExportEmissive");
        case EMaterialShaderMapUsage::LightmassExportNormal: return TEXT("LightmassExportNormal");
        case EMaterialShaderMapUsage::LightmassExportOpacity: return TEXT("LightmassExportOpacity");
        case EMaterialShaderMapUsage::MaterialExportEmissive: return TEXT("MaterialExportEmissive");
        case EMaterialShaderMapUsage::MaterialExportMetallic: return TEXT("MaterialExportMetallic");
        case EMaterialShaderMapUsage::MaterialExportNormal: return TEXT("MaterialExportNormal");
        case EMaterialShaderMapUsage::MaterialExportOpacity: return TEXT("MaterialExportOpacity");
        case EMaterialShaderMapUsage::MaterialExportRoughness: return TEXT("MaterialExportRoughness");
        case EMaterialShaderMapUsage::MaterialExportSpecular: return TEXT("MaterialExportSpecular");
        case EMaterialShaderMapUsage::MaterialExportAO: return TEXT("MaterialExportAO");
        case EMaterialShaderMapUsage::MaterialExportBaseColor: return TEXT("MaterialExportBaseColor");
        case EMaterialShaderMapUsage::MaterialExportOpacityMask: return TEXT("MaterialExportOpacityMask");
        case EMaterialShaderMapUsage::MaterialExportSubSurfaceColor: return TEXT("MaterialExportSubSurfaceColor");
        default: checkf(0, TEXT("Unsuppported EMaterialShaderMapUsage: %d"), MaterialShaderMapUsage); return TEXT("");
    }
}

FString UMaterialAssetSerializer::ShaderFrequencyToString(uint8 ShaderFrequency) {
    switch (ShaderFrequency) {
        case EShaderFrequency::SF_Compute: return TEXT("SF_Compute");
        case EShaderFrequency::SF_Domain: return TEXT("SF_Domain");
        case EShaderFrequency::SF_Geometry: return TEXT("SF_Geometry");
        case EShaderFrequency::SF_Hull: return TEXT("SF_Hull");
        case EShaderFrequency::SF_Pixel: return TEXT("SF_Pixel");
        case EShaderFrequency::SF_Vertex: return TEXT("SF_Vertex");
        case EShaderFrequency::SF_RayGen: return TEXT("SF_RayGen");
        case EShaderFrequency::SF_RayMiss: return TEXT("SF_RayMiss");
        case EShaderFrequency::SF_RayHitGroup: return TEXT("SF_RayHitGroup");
        default: checkf(0, TEXT("Unsupported shader frequency: %d"), ShaderFrequency); return TEXT("");
    }
}


FString UMaterialAssetSerializer::ShaderPlatformToString(uint8 ShaderPlatform) {
    switch (ShaderPlatform) {
        case SP_PCD3D_SM5: return TEXT("PCD3D_SM5");
        case SP_OPENGL_SM4: return TEXT("OPENGL_SM4");
        case SP_PS4: return TEXT("OPENGL_SM4");
        case SP_OPENGL_PCES2: return TEXT("OPENGL_PCES2");
        case SP_XBOXONE_D3D12: return TEXT("XBOXONE_D3D12");
        case SP_PCD3D_SM4: return TEXT("PCD3D_SM4");
        case SP_OPENGL_SM5: return TEXT("OPENGL_SM5");
        case SP_PCD3D_ES2: return TEXT("PCD3D_ES2");
        case SP_OPENGL_ES2_ANDROID: return TEXT("OPENGL_ES2_ANDROID");
        case SP_OPENGL_ES2_WEBGL: return TEXT("OPENGL_ES2_WEBGL");
        case SP_OPENGL_ES2_IOS: return TEXT("OPENGL_ES2_IOS");
        case SP_METAL: return TEXT("METAL");
        case SP_METAL_MRT: return TEXT("METAL_MRT");
        case SP_METAL_TVOS: return TEXT("METAL_TVOS");
        case SP_METAL_MRT_TVOS: return TEXT("METAL_MRT_TVOS");
        case SP_OPENGL_ES31_EXT: return TEXT("OPENGL_ES31_EXT");
        case SP_PCD3D_ES3_1: return TEXT("PCD3D_ES3_1");
        case SP_OPENGL_PCES3_1: return TEXT("OPENGL_PCES3_1");
        case SP_METAL_SM5: return TEXT("METAL_SM5");
        case SP_VULKAN_PCES3_1: return TEXT("VULKAN_PCES3_1");
        case SP_METAL_SM5_NOTESS: return TEXT("METAL_SM5_NOTESS");
        case SP_VULKAN_SM4: return TEXT("VULKAN_SM4");
        case SP_VULKAN_SM5: return TEXT("VULKAN_SM5");
        case SP_VULKAN_ES3_1_ANDROID: return TEXT("VULKAN_ES3_1_ANDROID");
        case SP_METAL_MACES3_1: return TEXT("METAL_MACES3_1");
        case SP_METAL_MACES2: return TEXT("METAL_MACES2");
        case SP_OPENGL_ES3_1_ANDROID: return TEXT("OPENGL_ES3_1_ANDROID");
        case SP_SWITCH: return TEXT("SWITCH");
        case SP_SWITCH_FORWARD: return TEXT("SWITCH_FORWARD");
        case SP_METAL_MRT_MAC: return TEXT("METAL_MRT_MAC");
        default: checkf(0, TEXT("Unknown shader platform: %d"), ShaderPlatform); return TEXT("");
    }
}


void UMaterialAssetSerializer::SerializeMaterialValue(uint32 MaterialValueType, const FLinearColor& MaterialValue, TSharedPtr<FJsonObject> OutJson) {
    OutJson->SetStringField(TEXT("MaterialValueType"), MaterialValueTypeToString(MaterialValueType));
    OutJson->SetObjectField(TEXT("MaterialValue"), SerializeLinearColor(MaterialValue));
}

TSharedPtr<FJsonObject> UMaterialAssetSerializer::SerializeLinearColor(const FLinearColor& Color) {
    TSharedPtr<FJsonObject> MaterialValueObject = MakeShareable(new FJsonObject());
    MaterialValueObject->SetNumberField(TEXT("R"), Color.R);
    MaterialValueObject->SetNumberField(TEXT("G"), Color.G);
    MaterialValueObject->SetNumberField(TEXT("B"), Color.B);
    MaterialValueObject->SetNumberField(TEXT("A"), Color.A);
    return MaterialValueObject;
}

static FString ParameterAssociationToString(uint8 MaterialParameterAssociation) {
    switch (MaterialParameterAssociation) {
        case EMaterialParameterAssociation::BlendParameter: return TEXT("BlendParameter");
        case EMaterialParameterAssociation::GlobalParameter: return TEXT("GlobalParameter");
        case EMaterialParameterAssociation::LayerParameter: return TEXT("LayerParameter");
        default: checkf(0, TEXT("Unknown MaterialParameterAssociation: %d"), MaterialParameterAssociation); return TEXT("");
    }
}

TSharedPtr<FJsonObject> UMaterialAssetSerializer::SerializeParameterInfo(const FMaterialParameterInfo& ParameterInfo) {
    TSharedPtr<FJsonObject> OutObject = MakeShareable(new FJsonObject());
    OutObject->SetStringField(TEXT("ParameterName"), ParameterInfo.Name.ToString());
    OutObject->SetStringField(TEXT("ParameterAssociation"), ParameterAssociationToString(ParameterInfo.Association));
    OutObject->SetNumberField(TEXT("ParameterIndex"), ParameterInfo.Index);
    return OutObject;
}

EAssetCategory UMaterialAssetSerializer::GetAssetCategory() const {
    return EAssetCategory::EAC_Material;
}*/
