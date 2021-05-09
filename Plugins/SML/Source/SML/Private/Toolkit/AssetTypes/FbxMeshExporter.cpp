#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "AnimEncoding.h"
#include "SatisfactoryModLoader.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Animation/AnimSequence.h"
#include "Toolkit/AssetTypes/FbxDataConverter.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

FString GetNameForUVChannel(uint32 Index) {
    if (Index == 0) {
        return TEXT("uv");
    }
    return FString::Printf(TEXT("uv%d"), Index + 1);
}

void FFbxMeshExporter::RegisterStaticMeshCPUAccessHook() {
    // UObject system should be initialized at this point so we can change UStaticMesh CDO
    check(UObjectInitialized());
    GetMutableDefault<UStaticMesh>()->bAllowCPUAccess = true;
}

FbxManager* AllocateFbxManagerForExport() {
    FbxManager* FbxManager = FbxManager::Create();
    check(FbxManager);

    // Initialize root I/O settings for created manager
    FbxIOSettings* IOSettings = FbxIOSettings::Create(FbxManager, IOSROOT);
    check(IOSettings);

    // IOSettings->SetBoolProp(EXP_FBX_MATERIAL, true);
    IOSettings->SetBoolProp(EXP_FBX_TEXTURE, true);
    IOSettings->SetBoolProp(EXP_FBX_SHAPE, true);
    IOSettings->SetBoolProp(EXP_FBX_GOBO, true);
    IOSettings->SetBoolProp(EXP_FBX_ANIMATION, true);
    IOSettings->SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

    FbxManager->SetIOSettings(IOSettings);
    return FbxManager;
}

FbxScene* CreateFbxSceneForFbxManager(FbxManager* FbxManager) {
    FbxScene* Scene = FbxScene::Create(FbxManager, "");

    // create scene info
    FbxDocumentInfo* SceneInfo = FbxDocumentInfo::Create(FbxManager, "SceneInfo");
    SceneInfo->mTitle = "SML FBX Exporter";
    SceneInfo->mComment = "All rights of exported game assets belong to CoffeeStain Studios. Do not redistribute.";

    const FbxString VersionString = FFbxDataConverter::ConvertToFbxString(FSatisfactoryModLoader::GetModLoaderVersion().ToString());
    SceneInfo->Original_ApplicationName.Set("Satisfactory Mod Loader");
    SceneInfo->Original_ApplicationVersion.Set(VersionString);
    Scene->SetSceneInfo(SceneInfo);

    FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
    const FbxAxisSystem UnrealZUp(FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded);
    Scene->GetGlobalSettings().SetAxisSystem(UnrealZUp);
    Scene->GetGlobalSettings().SetOriginalUpAxis(UnrealZUp);
    Scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::cm);
    Scene->GetGlobalSettings().SetTimeMode(FbxTime::eDefaultMode);

    return Scene;
}

bool ExportFbxSceneToFileByPath(const FString& OutFileName, FbxScene* Scene, bool bExportAsText, FString* OutErrorMessage) {
    FbxManager* RootManager = Scene->GetFbxManager();
    FbxExporter* FbxExporter = FbxExporter::Create(RootManager, "");
    FbxIOSettings* IOSettings = RootManager->GetIOSettings();

    int32 FileFormat;
    IOSettings->SetBoolProp(EXP_ASCIIFBX, bExportAsText);
    if (bExportAsText) {
        FileFormat = RootManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX ascii (*.fbx)");
    } else {
        FileFormat = RootManager->GetIOPluginRegistry()->GetNativeWriterFormat();
    }

    const FbxString FbxFileName = FFbxDataConverter::ConvertToFbxString(OutFileName);
    bool bSuccess = FbxExporter->Initialize(FbxFileName, FileFormat, IOSettings);

    if (!bSuccess) {
        if (OutErrorMessage) {
            *OutErrorMessage = UTF8_TO_TCHAR(FbxExporter->GetStatus().GetErrorString());
        }
        return false;
    }

    // Export scene with fbx mesh we created from static mesh
    bSuccess = FbxExporter->Export(Scene);
    if (!bSuccess) {
        if (OutErrorMessage) {
            *OutErrorMessage = UTF8_TO_TCHAR(FbxExporter->GetStatus().GetErrorString());
        }
        return false;
    }
    return true;
}

bool FFbxMeshExporter::ExportStaticMeshIntoFbxFile(UStaticMesh* StaticMesh, const FString& OutFileName, const bool bExportAsText, FString* OutErrorMessage) {
    // Make sure we either force static mesh data on CPU globally or mesh has it set locally
    check(StaticMesh->bAllowCPUAccess);
    FbxManager* FbxManager = AllocateFbxManagerForExport();
    check(FbxManager);

    // Create root scene which we will use to export mesh
    FbxScene* Scene = CreateFbxSceneForFbxManager(FbxManager);

    // Create mesh object
    const FbxString MeshNodeName = FFbxDataConverter::ConvertToFbxString(StaticMesh->GetName());
    FbxNode* MeshNode = FbxNode::Create(Scene, MeshNodeName);
    FbxMesh* OutExportedMesh = FbxMesh::Create(Scene, MeshNodeName);
    MeshNode->SetNodeAttribute(OutExportedMesh);

    const FStaticMeshLODResources& LODResources = StaticMesh->RenderData->LODResources[0];
    ExportStaticMesh(LODResources, StaticMesh->StaticMaterials, OutExportedMesh);

    Scene->GetRootNode()->AddChild(MeshNode);

    // Export scene into the file
    const bool bResult = ExportFbxSceneToFileByPath(OutFileName, Scene, bExportAsText, OutErrorMessage);

    // Destroy FbxManager, which will also destroy all objects allocated by it
    FbxManager->Destroy();
    return bResult;
}

bool FFbxMeshExporter::ExportSkeletonIntoFbxFile(USkeleton* Skeleton, const FString& OutFileName, bool bExportAsText, FString* OutErrorMessage) {
    FbxManager* FbxManager = AllocateFbxManagerForExport();
    check(FbxManager);

    // Create root scene which we will use to export mesh
    FbxScene* Scene = CreateFbxSceneForFbxManager(FbxManager);

    TArray<FbxNode*> BoneNodes;

    // Add the skeleton to the scene
    FbxNode* SkeletonRootNode = ExportSkeleton(Scene, Skeleton->GetReferenceSkeleton(), BoneNodes);
    Scene->GetRootNode()->AddChild(SkeletonRootNode);

    // Export scene into the file
    const bool bResult = ExportFbxSceneToFileByPath(OutFileName, Scene, bExportAsText, OutErrorMessage);

    // Destroy FbxManager, which will also destroy all objects allocated by it
    FbxManager->Destroy();
    return bResult;
}

bool FFbxMeshExporter::ExportSkeletalMeshIntoFbxFile(USkeletalMesh* SkeletalMesh, const FString& OutFileName, bool bExportAsText, FString* OutErrorMessage) {
    FbxManager* FbxManager = AllocateFbxManagerForExport();
    check(FbxManager);

    // Create root scene which we will use to export mesh
    FbxScene* Scene = CreateFbxSceneForFbxManager(FbxManager);

    // Create a temporary node attach to the scene root.
    // This will allow us to do the binding without the scene transform (non uniform scale is not supported when binding the skeleton)
    // We then detach from the temp node and attach to the parent and remove the temp node
    const FString FbxNodeName = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FbxNode* TmpNodeNoTransform = FbxNode::Create(Scene, TCHAR_TO_UTF8(*FbxNodeName));
    Scene->GetRootNode()->AddChild(TmpNodeNoTransform);

    TArray<FbxNode*> BoneNodes;

    // Add the skeleton to the scene
    FbxNode* SkeletonRootNode = ExportSkeleton(Scene, SkeletalMesh->RefSkeleton, BoneNodes);
    if (SkeletonRootNode) {
        TmpNodeNoTransform->AddChild(SkeletonRootNode);
    }

    // Create mesh from first LOD of the skeletal mesh
    const FSkeletalMeshLODRenderData& LODRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData[0];

    const FbxString MeshNodeName = FFbxDataConverter::ConvertToFbxString(SkeletalMesh->GetName());
    FbxNode* MeshRootNode = FbxNode::Create(Scene, MeshNodeName);
    FbxMesh* ExportedMesh = FbxMesh::Create(Scene, MeshNodeName);
    MeshRootNode->SetNodeAttribute(ExportedMesh);

    // Populate basic mesh information
    ExportSkeletalMesh(LODRenderData, SkeletalMesh->Materials, ExportedMesh);

    TmpNodeNoTransform->AddChild(MeshRootNode);

    if (SkeletonRootNode) {
        // Bind the mesh to the skeleton
        BindSkeletalMeshToSkeleton(LODRenderData, BoneNodes, MeshRootNode);

        // Add the bind pose
        CreateBindPose(MeshRootNode);
    }

    // Re-bind skeleton to scene root if we have one
    if (SkeletonRootNode) {
        // TmpNodeNoTransform->RemoveChild(SkeletonRootNode);
        Scene->GetRootNode()->AddChild(SkeletonRootNode);
    }

    // Re-bind mesh to the scene root
    TmpNodeNoTransform->RemoveChild(MeshRootNode);
    Scene->GetRootNode()->AddChild(MeshRootNode);

    // Remove old temporary node
    Scene->GetRootNode()->RemoveChild(TmpNodeNoTransform);
    Scene->RemoveNode(TmpNodeNoTransform);

    // Export scene into the file
    const bool bResult = ExportFbxSceneToFileByPath(OutFileName, Scene, bExportAsText, OutErrorMessage);

    // Destroy FbxManager, which will also destroy all objects allocated by it
    FbxManager->Destroy();
    return bResult;
}

bool FFbxMeshExporter::ExportAnimSequenceIntoFbxFile(UAnimSequence* AnimSequence, const FString& OutFileName, bool bExportAsText, FString* OutErrorMessage) {
    FbxManager* FbxManager = AllocateFbxManagerForExport();
    check(FbxManager);

    // Create root scene which we will use to export mesh
    FbxScene* Scene = CreateFbxSceneForFbxManager(FbxManager);

    // Create FBX animation stack and one base layer
    FbxAnimStack* AnimStack = FbxAnimStack::Create(Scene, "Unreal Animation Stack");
    const FString Description = FString::Printf(TEXT("Animation generated from Unreal Animation Sequence %s"), *AnimSequence->GetOutermost()->GetName());
    AnimStack->Description.Set(FFbxDataConverter::ConvertToFbxString(Description));

    FbxAnimLayer* AnimLayer = FbxAnimLayer::Create(Scene, "Base Layer");
    AnimStack->AddMember(AnimLayer);

    // Create a temporary node attach to the scene root.
    // This will allow us to do the binding without the scene transform (non uniform scale is not supported when binding the skeleton)
    // We then detach from the temp node and attach to the parent and remove the temp node
    const FString FbxNodeName = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FbxNode* TmpNodeNoTransform = FbxNode::Create(Scene, TCHAR_TO_UTF8(*FbxNodeName));
    Scene->GetRootNode()->AddChild(TmpNodeNoTransform);

    TArray<FbxNode*> BoneNodes;

    USkeleton* Skeleton = AnimSequence->GetSkeleton();
    check(Skeleton);

    // Add the skeleton to the scene
    FbxNode* SkeletonRootNode = ExportSkeleton(Scene, Skeleton->GetReferenceSkeleton(), BoneNodes);
    if (SkeletonRootNode) {
        TmpNodeNoTransform->AddChild(SkeletonRootNode);
    }

    ExportAnimSequence(AnimSequence, BoneNodes, NULL, AnimStack, AnimLayer, 0.0f, 0.0f, 1.0f, 0.0f);
    CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);

    // Re-bind skeleton to scene root if we have one
    if (SkeletonRootNode) {
        TmpNodeNoTransform->RemoveChild(SkeletonRootNode);
        Scene->GetRootNode()->AddChild(SkeletonRootNode);
    }

    // Remove old temporary node
    Scene->GetRootNode()->RemoveChild(TmpNodeNoTransform);
    Scene->RemoveNode(TmpNodeNoTransform);

    // Export scene into the file
    const bool bResult = ExportFbxSceneToFileByPath(OutFileName, Scene, bExportAsText, OutErrorMessage);

    // Destroy FbxManager, which will also destroy all objects allocated by it
    FbxManager->Destroy();
    return bResult;
}

void FFbxMeshExporter::ExportAnimSequence(const UAnimSequence* AnimSeq, TArray<FbxNode*>& BoneNodes, USkeletalMesh* SkeletalMesh, FbxAnimStack* AnimStack, FbxAnimLayer* InAnimLayer, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime) {
    // stack allocator for extracting curve
    FMemMark Mark(FMemStack::Get());

    USkeleton* Skeleton = AnimSeq->GetSkeleton();
    check(Skeleton);
    check(SetupAnimStack(AnimSeq, AnimStack));

    // Prepare root anim curves data to be exported
    TArray<FName> AnimCurveNames;
    TMap<FName, FbxAnimCurve*> CustomCurveMap;
    const FSmartNameMapping* AnimCurveMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
    if (BoneNodes.Num() > 0 && AnimCurveMapping != NULL) {
        AnimCurveMapping->FillNameArray(AnimCurveNames);

        for (const FName& AnimCurveName : AnimCurveNames) {
            FbxProperty AnimCurveFbxProp = FbxProperty::Create(BoneNodes[0], FbxDoubleDT, TCHAR_TO_ANSI(*AnimCurveName.ToString()));
            AnimCurveFbxProp.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
            AnimCurveFbxProp.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
            FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve(InAnimLayer, true);
            CustomCurveMap.Add(AnimCurveName, AnimFbxCurve);
        }
    }

    ExportCustomAnimCurvesToFbx(CustomCurveMap, AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime);

    // Add the animation data to the bone nodes
    for (int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); BoneIndex++) {
        FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

        // Create the AnimCurves
        const uint32 NumberOfCurves = 9;
        FbxAnimCurve* Curves[NumberOfCurves];

        // Individual curves for translation, rotation and scaling
        Curves[0] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        Curves[1] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        Curves[2] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        Curves[3] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        Curves[4] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        Curves[5] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        Curves[6] = CurrentBoneNode->LclScaling.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        Curves[7] = CurrentBoneNode->LclScaling.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        Curves[8] = CurrentBoneNode->LclScaling.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        const int32 BoneTreeIndex = SkeletalMesh ? Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(SkeletalMesh, BoneIndex) : BoneIndex;
        int32 BoneTrackIndex = Skeleton->GetRawAnimationTrackIndex(BoneTreeIndex, AnimSeq);

        if (BoneTrackIndex == INDEX_NONE) {
            // If this sequence does not have a track for the current bone, then skip it
            continue;
        }

        for (FbxAnimCurve* Curve : Curves) {
            Curve->KeyModifyBegin();
        }

        auto ExportLambda = [&](float AnimTime, FbxTime ExportTime, bool bLastKey) {
            FTransform BoneAtom;
            AnimSeq->GetBoneTransform(BoneAtom, BoneTrackIndex, AnimTime, true);

            const FbxVector4 Translation = FFbxDataConverter::ConvertToFbxPos(BoneAtom.GetTranslation());
            const FbxVector4 Rotation = FFbxDataConverter::ConvertToFbxRot(BoneAtom.GetRotation().Euler());
            const FbxVector4 Scale = FFbxDataConverter::ConvertToFbxScale(BoneAtom.GetScale3D());
            FbxVector4 Vectors[3] = {Translation, Rotation, Scale};

            // Loop over each curve and channel to set correct values
            for (uint32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex) {
                for (uint32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex) {
                    const uint32 OffsetCurveIndex = (CurveIndex * 3) + ChannelIndex;

                    const int32 lKeyIndex = Curves[OffsetCurveIndex]->KeyAdd(ExportTime);
                    Curves[OffsetCurveIndex]->KeySetValue(lKeyIndex, Vectors[CurveIndex][ChannelIndex]);
                    Curves[OffsetCurveIndex]->KeySetInterpolation(lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic);

                    if (bLastKey) {
                        Curves[OffsetCurveIndex]->KeySetConstantMode(lKeyIndex, FbxAnimCurveDef::eConstantStandard);
                    }
                }
            }
        };

        IterateInsideAnimSequence(AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, ExportLambda);

        for (FbxAnimCurve* Curve : Curves) {
            Curve->KeyModifyEnd();
        }
    }
}

bool FFbxMeshExporter::SetupAnimStack(const UAnimSequence* AnimSequence, FbxAnimStack* AnimStack) {
    check(AnimSequence->SequenceLength > 0.0f);
    const float FrameRate = FMath::TruncToFloat(((AnimSequence->GetRawNumberOfFrames() - 1) / AnimSequence->SequenceLength) + 0.5f);

    // Configure the scene time line
    FbxGlobalSettings& SceneGlobalSettings = AnimStack->GetScene()->GetGlobalSettings();
    const FbxTime::EMode ComputeTimeMode = FbxTime::ConvertFrameRateToTimeMode(FrameRate);
    FbxTime::SetGlobalTimeMode(ComputeTimeMode, ComputeTimeMode == FbxTime::eCustom ? FrameRate : 0.0);
    SceneGlobalSettings.SetTimeMode(ComputeTimeMode);

    if (ComputeTimeMode == FbxTime::eCustom) {
        SceneGlobalSettings.SetCustomFrameRate(FrameRate);
    }

    // set time correctly
    FbxTime ExportedStartTime, ExportedStopTime;
    ExportedStartTime.SetSecondDouble(0.0f);
    ExportedStopTime.SetSecondDouble(AnimSequence->SequenceLength);

    FbxTimeSpan ExportedTimeSpan;
    ExportedTimeSpan.Set(ExportedStartTime, ExportedStopTime);
    AnimStack->SetLocalTimeSpan(ExportedTimeSpan);

    return true;
}

void FFbxMeshExporter::IterateInsideAnimSequence(const UAnimSequence* AnimSeq, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, TFunctionRef<void(float, FbxTime, bool)> IterationLambda) {
    float AnimTime = AnimStartOffset;
    const float AnimEndTime = (AnimSeq->SequenceLength - AnimEndOffset);
    // Subtracts 1 because NumFrames includes an initial pose for 0.0 second

    const double TimePerKey = (AnimSeq->SequenceLength / (AnimSeq->GetRawNumberOfFrames() - 1));
    const float AnimTimeIncrement = TimePerKey * AnimPlayRate;
    uint32 AnimFrameIndex = 0;

    FbxTime ExportTime;
    ExportTime.SetSecondDouble(StartTime);

    FbxTime ExportTimeIncrement;
    ExportTimeIncrement.SetSecondDouble(TimePerKey);

    // Step through each frame and add custom curve data
    bool bLastKey = false;
    while (!bLastKey) {
        bLastKey = (AnimTime + KINDA_SMALL_NUMBER) > AnimEndTime;

        IterationLambda(AnimTime, ExportTime, bLastKey);

        ExportTime += ExportTimeIncrement;
        AnimFrameIndex++;
        AnimTime = AnimStartOffset + ((float)AnimFrameIndex * AnimTimeIncrement);
    }
}

void FFbxMeshExporter::ExportCustomAnimCurvesToFbx(const TMap<FName, FbxAnimCurve*>& CustomCurves, const UAnimSequence* AnimSequence, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime) {
    // stack allocator for extracting curve
    FMemMark Mark(FMemStack::Get());

    const USkeleton* Skeleton = AnimSequence->GetSkeleton();
    check(Skeleton);

    const FSmartNameMapping* SmartNameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
    check(SmartNameMapping);

    TArray<SmartName::UID_Type> AnimCurveUIDs;

    // We need to recreate the UIDs array manually so that we keep the empty entries otherwise the BlendedCurve won't have the correct mapping.
    // TODO probably not needed because all empty entries are stripped in shipping during cooking
    TArray<FName> UIDToNameArray;
    SmartNameMapping->FillUIDToNameArray(UIDToNameArray);
    AnimCurveUIDs.Reserve(UIDToNameArray.Num());
    for (int32 NameIndex = 0; NameIndex < UIDToNameArray.Num(); ++NameIndex) {
        AnimCurveUIDs.Add(NameIndex);
    }

    for (const TTuple<FName, FbxAnimCurve*>& CustomCurve : CustomCurves) {
        CustomCurve.Value->KeyModifyBegin();
    }

    auto ExportLambda = [&](float AnimTime, FbxTime ExportTime, bool bLastKey) {
        FBlendedCurve BlendedCurve;
        BlendedCurve.InitFrom(&AnimCurveUIDs);
        AnimSequence->EvaluateCurveData(BlendedCurve, AnimTime, true);

        if (BlendedCurve.IsValid()) {
            // Loop over the custom curves and add the actual keys
            for (auto CustomCurve : CustomCurves) {
                const SmartName::UID_Type NameUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CustomCurve.Key);

                if (NameUID != SmartName::MaxUID) {
                    const float CurveValueAtTime = BlendedCurve.Get(NameUID);
                    const int32 KeyIndex = CustomCurve.Value->KeyAdd(ExportTime);
                    CustomCurve.Value->KeySetValue(KeyIndex, CurveValueAtTime);
                }
            }
        }
    };

    IterateInsideAnimSequence(AnimSequence, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, ExportLambda);

    for (const TTuple<FName, FbxAnimCurve*>& CustomCurve : CustomCurves) {
        CustomCurve.Value->KeyModifyEnd();
    }
}

void FFbxMeshExporter::CorrectAnimTrackInterpolation(TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer) {
    // Add the animation data to the bone nodes
    for (int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex) {
        FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

        // Fetch the AnimCurves
        FbxAnimCurve* Curves[3];
        Curves[0] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        Curves[1] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        Curves[2] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        for (int32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex) {
            FbxAnimCurve* CurrentCurve = Curves[CurveIndex];
            CurrentCurve->KeyModifyBegin();

            float CurrentAngleOffset = 0.f;
            for (int32 KeyIndex = 1; KeyIndex < CurrentCurve->KeyGetCount(); KeyIndex++) {
                const float PreviousOutVal = CurrentCurve->KeyGetValue(KeyIndex - 1);
                float CurrentOutVal = CurrentCurve->KeyGetValue(KeyIndex);
                const float DeltaAngle = (CurrentOutVal + CurrentAngleOffset) - PreviousOutVal;

                if (DeltaAngle >= 180) {
                    CurrentAngleOffset -= 360;
                } else if (DeltaAngle <= -180) {
                    CurrentAngleOffset += 360;
                }

                CurrentOutVal += CurrentAngleOffset;
                CurrentCurve->KeySetValue(KeyIndex, CurrentOutVal);
            }

            CurrentCurve->KeyModifyEnd();
        }
    }
}

FbxNode* FFbxMeshExporter::ExportSkeleton(FbxScene* Scene, const FReferenceSkeleton& Skeleton, TArray<FbxNode*>& BoneNodes) {
    // Return NULL if we don't have any bones in the skeleton
    if (Skeleton.GetRawBoneNum() == 0) {
        return NULL;
    }

    // Create list of all skeleton nodes, and then link them according to skeleton bone hierarchy
    BoneNodes.Reserve(Skeleton.GetRawBoneNum());

    for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetRawBoneNum(); ++BoneIndex) {

        const FMeshBoneInfo& CurrentBone = Skeleton.GetRawRefBoneInfo()[BoneIndex];
        const FTransform& BoneTransform = Skeleton.GetRawRefBonePose()[BoneIndex];

        FbxString BoneName = FFbxDataConverter::ConvertToFbxString(CurrentBone.Name);

        // Create the node's attributes
        FbxSkeleton* SkeletonAttribute = FbxSkeleton::Create(Scene, BoneName.Buffer());
        if (BoneIndex) {
            SkeletonAttribute->SetSkeletonType(FbxSkeleton::eLimbNode);
        } else {
            // First bone in the skeleton bone's list is a root one
            SkeletonAttribute->SetSkeletonType(FbxSkeleton::eRoot);
        }

        // Create the node
        FbxNode* BoneNode = FbxNode::Create(Scene, BoneName.Buffer());
        BoneNode->SetNodeAttribute(SkeletonAttribute);

        // Set the bone node's local orientation
        const FVector UnrealRotation = BoneTransform.GetRotation().Euler();
        FbxVector4 LocalPos = FFbxDataConverter::ConvertToFbxPos(BoneTransform.GetTranslation());
        FbxVector4 LocalRot = FFbxDataConverter::ConvertToFbxRot(UnrealRotation);
        FbxVector4 LocalScale = FFbxDataConverter::ConvertToFbxScale(BoneTransform.GetScale3D());

        BoneNode->LclTranslation.Set(LocalPos);
        BoneNode->LclRotation.Set(LocalRot);
        BoneNode->LclScaling.Set(LocalScale);

        // If this is not the root bone, attach it to its parent
        if (BoneIndex) {
            BoneNodes[CurrentBone.ParentIndex]->AddChild(BoneNode);
        }
        // Add the node to the list of nodes, in bone order
        BoneNodes.Push(BoneNode);
    }

    return BoneNodes[0];
}

void FFbxMeshExporter::ExportCommonMeshResources(const FStaticMeshVertexBuffers& VertexBuffers, FbxMesh* FbxMesh) {
    // Initialize vertices first
    const uint32 NumVertices = VertexBuffers.PositionVertexBuffer.GetNumVertices();
    FbxMesh->InitControlPoints(NumVertices);
    FbxVector4* ControlPoints = FbxMesh->GetControlPoints();
    for (uint32 i = 0; i < NumVertices; i++) {
        const FVector& SrcPosition = VertexBuffers.PositionVertexBuffer.VertexPosition(i);
        FbxVector4& DestPosition = ControlPoints[i];
        DestPosition = FFbxDataConverter::ConvertToFbxPos(SrcPosition);
    }

    // Initialize vertex colors (if we have any)
    if (VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0) {
        check(VertexBuffers.ColorVertexBuffer.GetNumVertices() == NumVertices);
        FbxGeometryElementVertexColor* VertexColor = FbxMesh->CreateElementVertexColor();
        VertexColor->SetMappingMode(FbxLayerElement::eByControlPoint);
        VertexColor->SetReferenceMode(FbxLayerElement::eDirect);
        FbxLayerElementArrayTemplate<FbxColor>& ColorArray = VertexColor->GetDirectArray();
        ColorArray.AddMultiple(NumVertices);

        for (uint32 i = 0; i < NumVertices; i++) {
            const FColor& SrcColor = VertexBuffers.ColorVertexBuffer.VertexColor(i);
            ColorArray.SetAt(i, FFbxDataConverter::ConvertToFbxColor(SrcColor));
        }
    }

    // Initialize normals
    check(VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == NumVertices);
    FbxGeometryElementNormal* Normal = FbxMesh->CreateElementNormal();
    Normal->SetMappingMode(FbxLayerElement::eByControlPoint);
    Normal->SetReferenceMode(FbxLayerElement::eDirect);
    FbxLayerElementArrayTemplate<FbxVector4>& NormalArray = Normal->GetDirectArray();
    NormalArray.AddMultiple(NumVertices);

    for (uint32 i = 0; i < NumVertices; i++) {
        const FVector4 SrcNormal = VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(i);
        FbxVector4 DestNormal = FFbxDataConverter::ConvertToFbxPos(SrcNormal);
        NormalArray.SetAt(i, DestNormal);
    }

    // Initialize tangents
    FbxGeometryElementTangent* Tangent = FbxMesh->CreateElementTangent();
    Tangent->SetMappingMode(FbxLayerElement::eByControlPoint);
    Tangent->SetReferenceMode(FbxLayerElement::eDirect);
    FbxLayerElementArrayTemplate<FbxVector4>& TangentArray = Tangent->GetDirectArray();
    TangentArray.AddMultiple(NumVertices);

    for (uint32 i = 0; i < NumVertices; i++) {
        const FVector4 SrcTangent = VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(i);
        FbxVector4 DestTangent = FFbxDataConverter::ConvertToFbxPos(SrcTangent);
        TangentArray.SetAt(i, DestTangent);
    }

    // Initialize binormals
    FbxGeometryElementBinormal* Binormal = FbxMesh->CreateElementBinormal();
    Binormal->SetMappingMode(FbxLayerElement::eByControlPoint);
    Binormal->SetReferenceMode(FbxLayerElement::eDirect);
    FbxLayerElementArrayTemplate<FbxVector4>& BinormalArray = Binormal->GetDirectArray();
    BinormalArray.AddMultiple(NumVertices);

    for (uint32 i = 0; i < NumVertices; i++) {
        const FVector4 SrcBinormal = VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(i);
        FbxVector4 DestBinormal = FFbxDataConverter::ConvertToFbxPos(SrcBinormal);
        BinormalArray.SetAt(i, DestBinormal);
    }

    // Initialize UV positions for each channel
    const uint32 NumTexCoords = VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
    TArray<FbxLayerElementArrayTemplate<FbxVector2>*> UVCoordsArray;
    for (uint32 i = 0; i < NumTexCoords; i++) {
        // TODO proper names, can know if texture is lightmap by checking lightmap tex coord index from static mesh
        const FString UVChannelName = GetNameForUVChannel(i);
        FbxGeometryElementUV* UVCoords = FbxMesh->CreateElementUV(FFbxDataConverter::ConvertToFbxString(UVChannelName));
        UVCoords->SetMappingMode(FbxLayerElement::eByControlPoint);
        UVCoords->SetReferenceMode(FbxLayerElement::eDirect);
        FbxLayerElementArrayTemplate<FbxVector2>* TexCoordArray = &UVCoords->GetDirectArray();
        TexCoordArray->AddMultiple(NumVertices);

        UVCoordsArray.Add(TexCoordArray);
    }

    // Populate UV coords for each vertex
    for (uint32 j = 0; j < NumTexCoords; j++) {
        FbxLayerElementArrayTemplate<FbxVector2>* UVArray = UVCoordsArray[j];
        for (uint32 i = 0; i < NumVertices; i++) {
            const FVector2D& SrcTextureCoord = VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, j);
            FbxVector2 DestUVCoord(SrcTextureCoord.X, SrcTextureCoord.Y);
            UVArray->SetAt(i, DestUVCoord);
        }
    }
}

int32 ExportDummyMaterialIntoFbxScene(const FString& MaterialSlotName, FbxNode* Node) {
    // Create dummy material
    // MaterialSlotName will be either current material name or predefined name
    FbxScene* Scene = Node->GetScene();
    FbxSurfaceMaterial* DummySectionMaterial = FbxSurfaceMaterial::Create(Scene, FFbxDataConverter::ConvertToFbxString(MaterialSlotName));
    check(DummySectionMaterial);
    return Node->AddMaterial(DummySectionMaterial);
}

void AddNodeRecursively(TArray<FbxNode*>& OutNodeArray, FbxNode* Node) {
    if (Node) {
        AddNodeRecursively(OutNodeArray, Node->GetParent());
        OutNodeArray.AddUnique(Node);
    }
}

void FFbxMeshExporter::CreateBindPose(FbxNode* MeshRootNode) {
    FbxScene* Scene = MeshRootNode->GetScene();

    // In the bind pose, we must store all the link's global matrix at the time of the bind.
    // Plus, we must store all the parent(s) global matrix of a link, even if they are not
    // themselves deforming any model.

    // Create a bind pose with the link list
    TArray<FbxNode*> ClusteredFbxNodes;

    FbxNodeAttribute* NodeAttribute = MeshRootNode->GetNodeAttribute();
    if (NodeAttribute) {
        int32 SkinCount = 0;
        int32 ClusterCount = 0;

        if (NodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh ||
            NodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs ||
            NodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch) {

            FbxGeometry* GeometryAttribute = (FbxGeometry*)NodeAttribute;
            SkinCount = GeometryAttribute->GetDeformerCount(FbxDeformer::eSkin);

            // Go through all the skins and count them
            // then go through each skin and get their cluster count

            for (int32 i = 0; i < SkinCount; i++) {
                FbxSkin* Skin = (FbxSkin*)GeometryAttribute->GetDeformer(i, FbxDeformer::eSkin);
                ClusterCount += Skin->GetClusterCount();
            }
        }

        // if we found some clusters we must add the node
        if (ClusterCount) {
            // Again, go through all the skins get each cluster link and add them
            for (int32 i = 0; i < SkinCount; i++) {
                FbxGeometry* GeometryAttribute = (FbxGeometry*)NodeAttribute;
                FbxSkin* Skin = (FbxSkin*)GeometryAttribute->GetDeformer(i, FbxDeformer::eSkin);

                for (int32 j = 0; j < Skin->GetClusterCount(); j++) {
                    FbxNode* SkinClusterNode = Skin->GetCluster(j)->GetLink();
                    AddNodeRecursively(ClusteredFbxNodes, SkinClusterNode);
                }
            }
            // Add the patch to the pose
            ClusteredFbxNodes.Add(MeshRootNode);
        }
    }

    // Now create a bind pose with the link list
    if (ClusteredFbxNodes.Num()) {
        // A pose must be named. Arbitrarily use the name of the patch node.
        FbxPose* Pose = FbxPose::Create(Scene, MeshRootNode->GetName());

        // default pose type is rest pose, so we need to set the type as bind pose
        Pose->SetIsBindPose(true);

        for (FbxNode* FbxNode : ClusteredFbxNodes) {
            FbxMatrix BindMatrix = FbxNode->EvaluateGlobalTransform();
            Pose->Add(FbxNode, BindMatrix);
        }

        // Add the pose to the scene
        Scene->AddPose(Pose);
    }
}

uint32 FSkinWeightDataVertexBuffer_GetBoneIndex(const FSkinWeightDataVertexBuffer& VertexBuffer, uint32 VertexWeightOffset, uint32 VertexInfluenceCount, uint32 InfluenceIndex) {
    if (InfluenceIndex < VertexInfluenceCount) {
        const uint8* BoneData = VertexBuffer.GetData() + VertexWeightOffset;
        if (VertexBuffer.Use16BitBoneIndex()) {
            FBoneIndex16* BoneIndex16Ptr = (FBoneIndex16*)BoneData;
            return BoneIndex16Ptr[InfluenceIndex];
        }
        return BoneData[InfluenceIndex];
    }
    return 0;
}

uint8 FSkinWeightDataVertexBuffer_GetBoneWeight(const FSkinWeightDataVertexBuffer& VertexBuffer, uint32 VertexWeightOffset, uint32 VertexInfluenceCount, uint32 InfluenceIndex) {
    if (InfluenceIndex < VertexInfluenceCount) {
        const uint8* BoneData = VertexBuffer.GetData() + VertexWeightOffset;
        uint32 BoneWeightOffset = VertexBuffer.GetBoneIndexByteSize() * VertexInfluenceCount;
        return BoneData[BoneWeightOffset + InfluenceIndex];
    }
    return 0;
}

void FSkinWeightLookupVertexBuffer_GetWeightOffsetAndInfluenceCount(const FSkinWeightLookupVertexBuffer& VertexBuffer, uint32 VertexIndex, uint32& OutWeightOffset, uint32& OutInfluenceCount) {
    uint32 Offset = VertexIndex * 4;
    uint32 DataUInt32 = *((const uint32*)(&VertexBuffer.GetData()[Offset]));
    OutWeightOffset = DataUInt32 >> 8;
    OutInfluenceCount = DataUInt32 & 0xff;
}

// Copied from FSkinWeightVertexBuffer methods because they are not exported by the engine
void FSkinWeightVertexBuffer_GetVertexInfluenceOffsetCount(const FSkinWeightVertexBuffer& Buffer, uint32 VertexIndex, uint32& VertexWeightOffset, uint32& VertexInfluenceCount) {
    if (Buffer.GetVariableBonesPerVertex()) {
        FSkinWeightLookupVertexBuffer_GetWeightOffsetAndInfluenceCount(*Buffer.GetLookupVertexBuffer(), VertexIndex, VertexWeightOffset, VertexInfluenceCount);
    } else {
        VertexWeightOffset = Buffer.GetConstantInfluencesVertexStride() * VertexIndex;
        VertexInfluenceCount = Buffer.GetMaxBoneInfluences();
    }
}

uint32 FSkinWeightVertexBuffer_GetBoneIndex(const FSkinWeightVertexBuffer& Buffer, uint32 VertexIndex, uint32 InfluenceIndex) {
    uint32 VertexWeightOffset = 0;
    uint32 VertexInfluenceCount = 0;
    FSkinWeightVertexBuffer_GetVertexInfluenceOffsetCount(Buffer, VertexIndex, VertexWeightOffset, VertexInfluenceCount);
    return FSkinWeightDataVertexBuffer_GetBoneIndex(*Buffer.GetDataVertexBuffer(), VertexWeightOffset, VertexInfluenceCount, InfluenceIndex);
}

uint8 FSkinWeightVertexBuffer_GetBoneWeight(const FSkinWeightVertexBuffer& Buffer, uint32 VertexIndex, uint32 InfluenceIndex) {
    uint32 VertexWeightOffset = 0;
    uint32 VertexInfluenceCount = 0;
    FSkinWeightVertexBuffer_GetVertexInfluenceOffsetCount(Buffer, VertexIndex, VertexWeightOffset, VertexInfluenceCount);
    return FSkinWeightDataVertexBuffer_GetBoneWeight(*Buffer.GetDataVertexBuffer(), VertexWeightOffset, VertexInfluenceCount, InfluenceIndex);
}

FSkinWeightInfo FSkinWeightVertexBuffer_GetVertexSkinWeights(const FSkinWeightVertexBuffer& Buffer, uint32 VertexIndex) {
    FSkinWeightInfo OutVertex;
    for (int32 InfluenceIdx = 0; InfluenceIdx < MAX_TOTAL_INFLUENCES; InfluenceIdx++) {
        OutVertex.InfluenceBones[InfluenceIdx] = FSkinWeightVertexBuffer_GetBoneIndex(Buffer, VertexIndex, InfluenceIdx);
        OutVertex.InfluenceWeights[InfluenceIdx] = FSkinWeightVertexBuffer_GetBoneWeight(Buffer, VertexIndex, InfluenceIdx);
    }
    return OutVertex;
}

void FFbxMeshExporter::BindSkeletalMeshToSkeleton(const FSkeletalMeshLODRenderData& SkeletalMeshLOD, const TArray<FbxNode*>& BoneNodes, FbxNode* MeshRootNode) {
    FbxScene* Scene = MeshRootNode->GetScene();
    FbxAMatrix MeshMatrix = MeshRootNode->EvaluateGlobalTransform();

    FbxGeometry* MeshAttribute = (FbxGeometry*)MeshRootNode->GetNodeAttribute();
    FbxSkin* Skin = FbxSkin::Create(Scene, "");
    const FSkinWeightVertexBuffer& SkinWeightVertexBuffer = SkeletalMeshLOD.SkinWeightVertexBuffer;

    const int32 BoneCount = BoneNodes.Num();
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex) {
        FbxNode* BoneNode = BoneNodes[BoneIndex];

        // Create the deforming cluster
        FbxCluster* CurrentCluster = FbxCluster::Create(Scene, "");
        CurrentCluster->SetLink(BoneNode);
        CurrentCluster->SetLinkMode(FbxCluster::eTotalOne);

        // We need to do it per-section because bone indices of vertex are local to the section they are contained in
        // So we can mesh bone index from section local bone index and then apply transform to this bone
        for (const FSkelMeshRenderSection& RenderSection : SkeletalMeshLOD.RenderSections) {
            const uint32 BaseVertexIndex = RenderSection.BaseVertexIndex;
            const uint32 MaxVertexIndex = BaseVertexIndex + RenderSection.NumVertices;

            for (uint32 VertexIndex = BaseVertexIndex; VertexIndex < MaxVertexIndex; VertexIndex++) {
                // Retrieve influences for this exact vertex
                FSkinWeightInfo WeightInfo = FSkinWeightVertexBuffer_GetVertexSkinWeights(SkinWeightVertexBuffer, VertexIndex);

                for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; InfluenceIndex++) {
                    const int32 InfluenceBone = RenderSection.BoneMap[WeightInfo.InfluenceBones[InfluenceIndex]];
                    const float InfluenceWeight = WeightInfo.InfluenceWeights[InfluenceIndex] / 255.f;

                    if (InfluenceBone == BoneIndex && InfluenceWeight > 0.0f) {
                        CurrentCluster->AddControlPointIndex(VertexIndex, InfluenceWeight);
                    }
                }
            }
        }

        // Now we have the Patch and the skeleton correctly positioned,
        // set the Transform and TransformLink matrix accordingly.
        CurrentCluster->SetTransformMatrix(MeshMatrix);
        FbxAMatrix LinkMatrix = BoneNode->EvaluateGlobalTransform();

        CurrentCluster->SetTransformLinkMatrix(LinkMatrix);

        // Add the clusters to the mesh by creating a skin and adding those clusters to that skin.
        Skin->AddCluster(CurrentCluster);
    }

    // Add the skin to the mesh after the clusters have been added
    MeshAttribute->AddDeformer(Skin);
}

void FFbxMeshExporter::ExportSkeletalMesh(const FSkeletalMeshLODRenderData& SkeletalMeshLOD, const TArray<FSkeletalMaterial>& ReferencedMaterials, FbxMesh* FbxMesh) {
    // Currently all skeletal mesh data is kept on CPU to support skeletal mesh merging functionality
    // At least that's what UE source code and comments seem to imply. So we just make sure it is the case here
    // We check skin weight buffer because it's the only buffer with NeedsCPUAccess accessible from outside
    checkf(SkeletalMeshLOD.SkinWeightVertexBuffer.GetNeedsCPUAccess(), TEXT("Cannot export skeletal mesh without CPU access to buffers"));

    // Initialize material element
    FbxGeometryElementMaterial* Material = FbxMesh->CreateElementMaterial();
    Material->SetMappingMode(FbxLayerElement::eByPolygon);
    Material->SetReferenceMode(FbxLayerElement::eIndexToDirect);

    // Create basic static mesh buffers
    ExportCommonMeshResources(SkeletalMeshLOD.StaticVertexBuffers, FbxMesh);

    const FRawStaticIndexBuffer16or32Interface* IndexBuffer = SkeletalMeshLOD.MultiSizeIndexContainer.GetIndexBuffer();
    FbxNode* MeshNode = FbxMesh->GetNode();

    // Create sections and initialize dummy materials
    for (const FSkelMeshRenderSection& MeshSection : SkeletalMeshLOD.RenderSections) {
        const uint32 NumTriangles = MeshSection.NumTriangles;
        const uint32 StartVertexIndex = MeshSection.BaseIndex;

        // Create dummy material for this section
        const FString MaterialSlotName = ReferencedMaterials[MeshSection.MaterialIndex].MaterialSlotName.ToString();
        const int32 MaterialIndex = ExportDummyMaterialIntoFbxScene(MaterialSlotName, MeshNode);

        // Add all triangles associated with this section
        for (uint32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++) {
            FbxMesh->BeginPolygon(MaterialIndex, -1, -1, false);
            FbxMesh->AddPolygon(IndexBuffer->Get(StartVertexIndex + TriangleIndex * 3 + 0));
            FbxMesh->AddPolygon(IndexBuffer->Get(StartVertexIndex + TriangleIndex * 3 + 1));
            FbxMesh->AddPolygon(IndexBuffer->Get(StartVertexIndex + TriangleIndex * 3 + 2));
            FbxMesh->EndPolygon();
        }
    }
}

void FFbxMeshExporter::ExportStaticMesh(const FStaticMeshLODResources& StaticMeshLOD, const TArray<FStaticMaterial>& ReferencedMaterials, FbxMesh* FbxMesh) {
    // Initialize material element
    FbxGeometryElementMaterial* Material = FbxMesh->CreateElementMaterial();
    Material->SetMappingMode(FbxLayerElement::eByPolygon);
    Material->SetReferenceMode(FbxLayerElement::eIndexToDirect);

    // Create basic static mesh buffers
    ExportCommonMeshResources(StaticMeshLOD.VertexBuffers, FbxMesh);

    const FRawStaticIndexBuffer& IndexBuffer = StaticMeshLOD.IndexBuffer;
    FbxNode* MeshNode = FbxMesh->GetNode();

    // Create sections and initialize dummy materials
    for (const FStaticMeshSection& MeshSection : StaticMeshLOD.Sections) {
        const uint32 NumTriangles = MeshSection.NumTriangles;
        const uint32 StartIndex = MeshSection.FirstIndex;

        // Create dummy material for this section
        const FString MaterialSlotName = ReferencedMaterials[MeshSection.MaterialIndex].MaterialSlotName.ToString();
        const int32 MaterialIndex = ExportDummyMaterialIntoFbxScene(MaterialSlotName, MeshNode);

        // Add all triangles associated with this section
        for (uint32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++) {
            FbxMesh->BeginPolygon(MaterialIndex, -1, -1, false);
            FbxMesh->AddPolygon(IndexBuffer.GetIndex(StartIndex + TriangleIndex * 3 + 0));
            FbxMesh->AddPolygon(IndexBuffer.GetIndex(StartIndex + TriangleIndex * 3 + 1));
            FbxMesh->AddPolygon(IndexBuffer.GetIndex(StartIndex + TriangleIndex * 3 + 2));
            FbxMesh->EndPolygon();
        }
    }
}
