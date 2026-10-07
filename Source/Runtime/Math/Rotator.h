#pragma once
#include "Math/EngineMath.h"

// Degree but Quat: Radians
struct FRotator
{
	union
	{
		float V[3];
		struct
		{
			float Pitch;
			float Yaw;
			float Roll;
		};
	};
	FRotator();
	FRotator(float P, float Y, float R);

	static FRotator Identitiy;

	FQuat Quaternion() const;

	float operator[] (int32 Index) const
	{
		return V[Index];
	}

	float& operator[] (int32 Index)
	{
		return V[Index];
	}

	FRotator operator+(const FRotator& Other)
	{
		Pitch += Other.Pitch;
		Yaw += Other.Yaw;
		Roll += Other.Roll;

		return *this;
	}

	FRotator operator*(const float Value) const
	{
		// 프레임 회전량을 구해도 원본 RotationRate는 변경하지 않는다.
		return FRotator(Pitch * Value, Yaw * Value, Roll * Value);
	}
};

FRotator operator+(FRotator Rot, const FVector& Vec);



