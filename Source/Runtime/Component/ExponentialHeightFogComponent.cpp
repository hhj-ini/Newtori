#include "EnginePCH.h"
#include "ExponentialHeightFogComponent.h"
#include "Engine/World.h"
#include <algorithm>

void UExponentialHeightFogComponent::ClampFogValues()
{
	FogDensity = std::max(0.0f, FogDensity);
	FogHeightFalloff = std::max(0.0f, FogHeightFalloff);
	StartDistance = std::max(0.0f, StartDistance);
	FogCutoffDistance = std::max(0.0f, FogCutoffDistance);
	FogMaxOpacity = std::clamp(FogMaxOpacity, 0.0f, 1.0f);
}

void UExponentialHeightFogComponent::OnTransformDirty()
{
	MarkFogDirty();
}

void UExponentialHeightFogComponent::MarkFogDirty()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;
	
	World->GetScene().MarkFogDirty(GetUUID());

}

void UExponentialHeightFogComponent::OnPropertyChanged(const FProperty& Property)
{
	Super::OnPropertyChanged(Property);

	const bool bFogFloat = Property.Name == "FogDensity" || Property.Name == "FogHeightFalloff" ||
		Property.Name == "StartDistance" || Property.Name == "FogCutoffDistance" ||
		Property.Name == "FogMaxOpacity";
	if (bFogFloat)
	{
		ClampFogValues();
	}

	if (bFogFloat || Property.Name == "FogInscatteringColor")
	{
		MarkFogDirty();
	}
}

void UExponentialHeightFogComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		ClampFogValues();
		MarkFogDirty();
	}
}
