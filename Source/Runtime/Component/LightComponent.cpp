#include "EnginePCH.h"
#include "LightComponent.h"
#include "Engine/World.h"

#include <algorithm>

void ULightComponent::ClampLightValues()
{
    Intensity = std::max(0.0f, Intensity);
}

void ULightComponent::SetIntensity(float InIntensity)
{
    Intensity = InIntensity;
    ClampLightValues();
    MarkLightDirty();
}

void ULightComponent::SetLightColor(const FVector4& InColor)
{
    LightColor = InColor;
    MarkLightDirty();
}

void ULightComponent::OnTransformDirty()
{
    Super::OnTransformDirty();
    MarkLightDirty();
}

void ULightComponent::OnPropertyChanged(const FProperty& Property)
{
    Super::OnPropertyChanged(Property);

    // 에디터는 Setter를 거치지 않으므로 여기에서도 처리한다.
    ClampLightValues();
    MarkLightDirty();
}

void ULightComponent::Serialize(json& Handle, bool bIsLoading)
{
    Super::Serialize(Handle, bIsLoading);

    if (bIsLoading)
    {
        ClampLightValues();
        MarkLightDirty();
    }
}
void ULightComponent::MarkLightDirty()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;
	World->GetScene().MarkLightDirty(GetUUID());	
}
