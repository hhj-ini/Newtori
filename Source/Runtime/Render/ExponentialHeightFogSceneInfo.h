#pragma once
#include <Math/Vector4.h>
#include "Component/ExponentialHeightFogComponent.h"




class FExponentialHeightFogSceneInfo
{
public:
	FExponentialHeightFogSceneInfo(UExponentialHeightFogComponent* InFog);
	void UpdateFromComponent(const UExponentialHeightFogComponent& InFog);

	// TODO : Use when implmenent volumetric fog
/*	struct FExponentialHeightFogSceneData
	{
		FVector4 FogInscatteringColor;
		float FogDensity;
		float FogHeightFalloff;
	};*/

	FVector4 FogInscatteringColor;
	float FogHeight;
	float FogDensity;
	float FogHeightFalloff;
	float StartDistance; 
	float FogCutoffDistance;
	float FogMaxOpacity;

};