#include "EnginePCH.h"
#include "Transform.h"

FTransform::FTransform(FVector InL, FVector InS, FRotator InR)
{
	Location = InL;
	Scale = InS;
	Rotation = InR;
}

FMatrix FTransform::GetLocalMatrix() const
{
    FMatrix RotationMatrix = Rotation.Quaternion().ToFMatrix();

    FMatrix WorldMatrix = FMatrix(
        RotationMatrix[0][0] * Scale.X, RotationMatrix[0][1] * Scale.X, RotationMatrix[0][2] * Scale.X, 0.0f,
        RotationMatrix[1][0] * Scale.Y, RotationMatrix[1][1] * Scale.Y, RotationMatrix[1][2] * Scale.Y, 0.0f,
        RotationMatrix[2][0] * Scale.Z, RotationMatrix[2][1] * Scale.Z, RotationMatrix[2][2] * Scale.Z, 0.0f,
        Location.X, Location.Y, Location.Z, 1.0f
    );

    return WorldMatrix;
}

// TRS 행렬에서 Location/Rotation/Scale을 다시 분해한다.
// 현재 행렬 규약은 Translation이 3번째 행에 있고 각 축 행의 길이가 Scale이다.
FTransform FTransform::FromMatrix(const FMatrix& Matrix)
{
    FTransform Result;
    Result.Location = FVector(Matrix[3][0], Matrix[3][1], Matrix[3][2]);

    FVector Row0(Matrix[0][0], Matrix[0][1], Matrix[0][2]);
    FVector Row1(Matrix[1][0], Matrix[1][1], Matrix[1][2]);
    FVector Row2(Matrix[2][0], Matrix[2][1], Matrix[2][2]);

    Result.Scale = FVector(Row0.Length(), Row1.Length(), Row2.Length());

    if (Result.Scale.X != 0.0f) Row0 /= Result.Scale.X;
    if (Result.Scale.Y != 0.0f) Row1 /= Result.Scale.Y;
    if (Result.Scale.Z != 0.0f) Row2 /= Result.Scale.Z;

    FMatrix RotationMatrix(
        Row0.X, Row0.Y, Row0.Z, 0.0f,
        Row1.X, Row1.Y, Row1.Z, 0.0f,
        Row2.X, Row2.Y, Row2.Z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    Result.Rotation = MatrixToRotator(RotationMatrix);

    return Result;
}

FVector FTransform::GetForward() const
{
    FQuat Quat = GetOrientation();
    return Quat.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FVector FTransform::GetUp() const
{
    FQuat Quat = GetOrientation();
    return Quat.RotateVector(FVector(0.0f, 0.0f, 1.0f));
}

FVector FTransform::GetRight() const
{
    return GetOrientation().RotateVector(FVector(0.0f, 1.0f, 0.0f));
}

FQuat FTransform::GetOrientation() const
{
    return FQuat::MakeFromEuler(FMath::DegreesToRadians(Rotation.Roll), FMath::DegreesToRadians(Rotation.Pitch), FMath::DegreesToRadians(Rotation.Yaw));
}

FTransform FTransform::Identity = FTransform(FVector(0, 0, 0), FVector(1, 1, 1), FRotator(0, 0, 0));