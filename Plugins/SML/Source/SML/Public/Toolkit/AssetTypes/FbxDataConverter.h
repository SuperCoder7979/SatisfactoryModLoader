#pragma once
#include "CoreMinimal.h"
#include "fbxsdk.h"

/** Handles data conversion between FBX and native UE formats */
class FFbxDataConverter {
public:
    static void SetJointPostConversionMatrix(FbxAMatrix ConversionMatrix) { JointPostConversionMatrix = ConversionMatrix; }
    static const FbxAMatrix& GetJointPostConversionMatrix() { return JointPostConversionMatrix; }

    static void SetAxisConversionMatrix(FbxAMatrix ConversionMatrix) {
        AxisConversionMatrix = ConversionMatrix;
        AxisConversionMatrixInv = ConversionMatrix.Inverse();
    }

    static const FbxAMatrix& GetAxisConversionMatrix() { return AxisConversionMatrix; }
    static const FbxAMatrix& GetAxisConversionMatrixInv() { return AxisConversionMatrixInv; }

    static FVector ConvertPos(FbxVector4 Vector);
    static FVector ConvertDir(FbxVector4 Vector);
    static FRotator ConvertEuler(FbxDouble3 Euler);
    static FVector ConvertScale(FbxDouble3 Vector);
    static FVector ConvertScale(FbxVector4 Vector);
    static FRotator ConvertRotation(FbxQuaternion Quaternion);
    static FVector ConvertRotationToFVect(FbxQuaternion Quaternion, bool bInvertRot);
    static FQuat ConvertRotToQuat(FbxQuaternion Quaternion);
    static float ConvertDist(FbxDouble Distance);
    static FTransform ConvertTransform(FbxAMatrix Matrix);
    static FMatrix ConvertMatrix(FbxAMatrix Matrix);

    /*
     * Convert fbx linear space color to sRGB FColor
     */
    static FColor ConvertColor(FbxDouble3 Color);

    static FbxVector4 ConvertToFbxPos(FVector Vector);
    static FbxVector4 ConvertToFbxRot(FVector Vector);
    static FbxVector4 ConvertToFbxScale(FVector Vector);

    /*
    * Convert sRGB FColor to fbx linear space color
    */
    static FbxDouble3 ConvertToFbxColor(FColor Color);
    static FbxString ConvertToFbxString(FName Name);
    static FbxString ConvertToFbxString(const FString& String);

    // FbxCamera with no rotation faces X with Y-up while ours faces X with Z-up so add a -90 degrees roll to compensate
    static FRotator GetCameraRotation() { return FRotator(0.f, 0.f, -90.f); }

    // FbxLight with no rotation faces -Z while ours faces Y so add a 90 degrees pitch to compensate
    static FRotator GetLightRotation() { return FRotator(0.f, 90.f, 0.f); }

private:
    static FbxAMatrix JointPostConversionMatrix;
    static FbxAMatrix AxisConversionMatrix;
    static FbxAMatrix AxisConversionMatrixInv;
};
