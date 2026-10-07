#include "EnginePCH.h"
#include "ExponentialHeightFogSceneInfo.h"

FExponentialHeightFogSceneInfo::FExponentialHeightFogSceneInfo(UExponentialHeightFogComponent* InFog)
{
	FExponentialHeightFogSceneInfo::UpdateFromComponent(*InFog);
}

void FExponentialHeightFogSceneInfo::UpdateFromComponent(const UExponentialHeightFogComponent& InFog)
{
	this->FogHeight = InFog.GetWorldLocation().Z;
	this->FogDensity = InFog.GetFogDensity();
	this->FogCutoffDistance = InFog.GetFogCutoffDistance();
	this->FogHeightFalloff = InFog.GetFogHeightFalloff();
	this->FogInscatteringColor = InFog.GetFogInscatteringColor();
	this->FogMaxOpacity = InFog.GetFogMaxOpacity();
	this->StartDistance = InFog.GetStartDistance();
}
