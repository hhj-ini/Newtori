#include "EnginePCH.h"
#include "ExponentialHeightFogComponent.h"
#include "Engine/World.h"

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

	if (Property.Name == "FogDensity" || Property.Name == "FogHeightFalloff" || Property.Name == "StartDistance" ||
		Property.Name == "FogCutoffDistance" || Property.Name == "FogMaxOpacity" || Property.Name == "FogInscatteringColor")
	{
		MarkFogDirty();
	}
}
