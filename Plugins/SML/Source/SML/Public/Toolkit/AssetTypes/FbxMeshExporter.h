#pragma once
#include "CoreMinimal.h"
#include "fbxsdk.h"

class UStaticMesh;
class USkeleton;
struct FStaticMaterial;
struct FStaticMeshLODResources;
class USkeletalMesh;
struct FSkeletalMaterial;
class FSkeletalMeshLODRenderData;
struct FReferenceSkeleton;
struct FStaticMeshVertexBuffers;
class UAnimSequence;

class SML_API FFbxMeshExporter {
public:
    /**
     * Hooks into UStaticMesh::Serialize to set bAllowCPUAccess to true
     * CPU access is needed to be able to dump meshes, because otherwise
     * their resources on CPU are freed once GPU resource is allocated
     */
    static void RegisterStaticMeshCPUAccessHook();

    /**
     * Exports static mesh data into the FBX file using the specified path
     * If exporting fails, false is returned and error message is populated with error message
     * Keep in mind that not all information is exported, and materials are not exported
     * Material slot names are kept intact during export though, and are filled with dummy materials
     */
    static bool ExportStaticMeshIntoFbxFile(UStaticMesh* StaticMesh, const FString& OutFileName, bool bExportAsText = false, FString* OutErrorMessage = NULL);

    /**
     * Exports skeleton itself into the FBX file
     * It does not actually export any geometry or animations, just a bare skeleton
     */
    static bool ExportSkeletonIntoFbxFile(USkeleton* Skeleton, const FString& OutFileName, bool bExportAsText = false, FString* OutErrorMessage = NULL);

    /**
     * Exports skeletal mesh into the FBX file
     * Overall behavior is similar to ExportStaticMeshIntoFbxFile, but
     * additional skeletal mesh related data (e.g skeleton, skin weights and binding pose) is exported
     * Animations are exported separately and this function does not handle that
     */
    static bool ExportSkeletalMeshIntoFbxFile(USkeletalMesh* SkeletalMesh, const FString& OutFileName, bool bExportAsText = false, FString* OutErrorMessage = NULL);

    /**
     * Exports animation sequence into the fbx file
     * Will export associated skeleton and animation applied to it,
     * but will not export any kind of skeletal meshes
     */
    static bool ExportAnimSequenceIntoFbxFile(UAnimSequence* AnimSequence, const FString& OutFileName, bool bExportAsText = false, FString* OutErrorMessage = NULL);
private:
    /** Exports animation sequence into the given fbx animation layer */
    static void ExportAnimSequence(const UAnimSequence* AnimSeq, TArray<FbxNode*>& BoneNodes, USkeletalMesh* SkeletalMesh, FbxAnimStack* AnimStack, FbxAnimLayer* InAnimLayer, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime);

    /** Setups animation stack for exporting unreal animations */
    static bool SetupAnimStack(const UAnimSequence* AnimSequence, FbxAnimStack* AnimStack);

    /** Exports custom animation curves into fbx curves */
    static void ExportCustomAnimCurvesToFbx(const TMap<FName, FbxAnimCurve*>& CustomCurves, const UAnimSequence* AnimSequence, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime);

    /** Iterates lambda inside of the anim sequence context passing various time values to it */
    static void IterateInsideAnimSequence(const UAnimSequence* AnimSeq, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, TFunctionRef<void(float, FbxTime, bool)> IterationLambda);

    /** Corrects interpolation on angle curves so that -179 -> 179 degrees interpolation doesn't go the whole circle */
    static void CorrectAnimTrackInterpolation(TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer);

    /** Exports Static Mesh LOD into the target fbx mesh object */
    static void ExportStaticMesh(const FStaticMeshLODResources& StaticMeshLOD, const TArray<FStaticMaterial>& ReferencedMaterials, FbxMesh* FbxMesh);

    /** Exports skeletal mesh LOD into the target fbx mesh */
    static void ExportSkeletalMesh(const FSkeletalMeshLODRenderData& SkeletalMeshLOD, const TArray<FSkeletalMaterial>& ReferencedMaterials, FbxMesh* FbxMesh);

    /** Exports skeleton into the fbx scene. Returns root skeleton bone and adds other nodes into the given array */
    static FbxNode* ExportSkeleton(FbxScene* Scene, const FReferenceSkeleton& Skeleton, TArray<FbxNode*>& BoneNodes);

    /** Creates bind pose for given fbx node holding mesh or surface */
    static void CreateBindPose(FbxNode* MeshRootNode);

    /** Binds skeletal mesh to skeleton by applying skin weights and skin deformation */
    static void BindSkeletalMeshToSkeleton(const FSkeletalMeshLODRenderData& SkeletalMeshLOD, const TArray<FbxNode*>& BoneNodes, FbxNode* MeshRootNode);

    /** Exports common mesh resources into FBX mesh */
    static void ExportCommonMeshResources(const FStaticMeshVertexBuffers& VertexBuffers, FbxMesh* FbxMesh);
};
