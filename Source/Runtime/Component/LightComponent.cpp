#include "EnginePCH.h"
#include "LightComponent.h"
#include "Engine/World.h"

#include <algorithm>

// 새 Actor뿐 아니라 기존 Actor에 추가하거나 복원한 Light도 Scene에 등록한다.
void ULightComponent::OnRegister()
{
	Super::OnRegister();
	GetOwner()->GetWorld()->GetScene().AddLight(this);
}

void ULightComponent::OnUnregister()
{
	GetOwner()->GetWorld()->GetScene().RemoveLight(this);
	Super::OnUnregister();
}

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
