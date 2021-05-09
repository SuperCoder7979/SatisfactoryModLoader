#include "Toolkit/AssetTypes/FbxDataConverter.h"

FbxAMatrix FFbxDataConverter::JointPostConversionMatrix;
FbxAMatrix FFbxDataConverter::AxisConversionMatrix;
FbxAMatrix FFbxDataConverter::AxisConversionMatrixInv;

FVector FFbxDataConverter::ConvertPos(FbxVector4 Vector) {
    FVector Out;
    Out[0] = Vector[0];
    // flip Y, then the right-handed axis system is converted to LHS
    Out[1] = -Vector[1];
    Out[2] = Vector[2];
    return Out;
}

FVector FFbxDataConverter::ConvertDir(FbxVector4 Vector) {
    FVector Out;
    Out[0] = Vector[0];
    Out[1] = -Vector[1];
    Out[2] = Vector[2];
    return Out;
}

FRotator FFbxDataConverter::ConvertEuler(FbxDouble3 Euler) {
    return FRotator::MakeFromEuler(FVector(Euler[0], -Euler[1], Euler[2]));
}

FVector FFbxDataConverter::ConvertScale(FbxDouble3 Vector) {
    FVector Out;
    Out[0] = Vector[0];
    Out[1] = Vector[1];
    Out[2] = Vector[2];
    return Out;
}

FVector FFbxDataConverter::ConvertScale(FbxVector4 Vector) {
    FVector Out;
    Out[0] = Vector[0];
    Out[1] = Vector[1];
    Out[2] = Vector[2];
    return Out;
}

FRotator FFbxDataConverter::ConvertRotation(FbxQuaternion Quaternion) {
    FRotator Out(ConvertRotToQuat(Quaternion));
    return Out;
}

FVector FFbxDataConverter::ConvertRotationToFVect(FbxQuaternion Quaternion, bool bInvertOrient) {
    FQuat UnrealQuaternion = ConvertRotToQuat(Quaternion);
    FVector Euler;
    Euler = UnrealQuaternion.Euler();
    if (bInvertOrient) {
        Euler.Y = -Euler.Y;
        Euler.Z = 180.f + Euler.Z;
    }
    return Euler;
}

FQuat FFbxDataConverter::ConvertRotToQuat(FbxQuaternion Quaternion) {
    FQuat UnrealQuat;
    UnrealQuat.X = Quaternion[0];
    UnrealQuat.Y = -Quaternion[1];
    UnrealQuat.Z = Quaternion[2];
    UnrealQuat.W = -Quaternion[3];

    return UnrealQuat;
}

float FFbxDataConverter::ConvertDist(FbxDouble Distance) {
    float Out;
    Out = (float)Distance;
    return Out;
}

FTransform FFbxDataConverter::ConvertTransform(FbxAMatrix Matrix) {
    FTransform Out;

    FQuat Rotation = ConvertRotToQuat(Matrix.GetQ());
    FVector Origin = ConvertPos(Matrix.GetT());
    FVector Scale = ConvertScale(Matrix.GetS());

    Out.SetTranslation(Origin);
    Out.SetScale3D(Scale);
    Out.SetRotation(Rotation);

    return Out;
}

FMatrix FFbxDataConverter::ConvertMatrix(FbxAMatrix Matrix) {
    FMatrix UEMatrix;

    for (int i = 0; i < 4; ++i) {
        FbxVector4 Row = Matrix.GetRow(i);
        if (i == 1) {
            UEMatrix.M[i][0] = -Row[0];
            UEMatrix.M[i][1] = Row[1];
            UEMatrix.M[i][2] = -Row[2];
            UEMatrix.M[i][3] = -Row[3];
        } else {
            UEMatrix.M[i][0] = Row[0];
            UEMatrix.M[i][1] = -Row[1];
            UEMatrix.M[i][2] = Row[2];
            UEMatrix.M[i][3] = Row[3];
        }
    }

    return UEMatrix;
}

FColor FFbxDataConverter::ConvertColor(FbxDouble3 Color) {
    // Fbx is in linear color space
    FColor SRGBColor = FLinearColor(Color[0], Color[1], Color[2]).ToFColor(true);
    return SRGBColor;
}

FbxVector4 FFbxDataConverter::ConvertToFbxPos(FVector Vector) {
    FbxVector4 Out{};
    Out[0] = Vector[0];
    Out[1] = -Vector[1];
    Out[2] = Vector[2];

    return Out;
}

FbxVector4 FFbxDataConverter::ConvertToFbxRot(FVector Vector) {
    FbxVector4 Out{};
    Out[0] = Vector[0];
    Out[1] = -Vector[1];
    Out[2] = -Vector[2];

    return Out;
}

FbxVector4 FFbxDataConverter::ConvertToFbxScale(FVector Vector) {
    FbxVector4 Out{};
    Out[0] = Vector[0];
    Out[1] = Vector[1];
    Out[2] = Vector[2];

    return Out;
}

FbxDouble3 FFbxDataConverter::ConvertToFbxColor(FColor Color) {
    // Fbx is in linear color space
    FLinearColor FbxLinearColor(Color);
    FbxDouble3 Out;
    Out[0] = FbxLinearColor.R;
    Out[1] = FbxLinearColor.G;
    Out[2] = FbxLinearColor.B;

    return Out;
}

FbxString FFbxDataConverter::ConvertToFbxString(FName Name) {
    FbxString OutString;

    FString UnrealString;
    Name.ToString(UnrealString);

    OutString = TCHAR_TO_UTF8(*UnrealString);

    return OutString;
}

FbxString FFbxDataConverter::ConvertToFbxString(const FString& String) {
    FbxString OutString;
    OutString = TCHAR_TO_UTF8(*String);

    return OutString;
}
