#include "EnginePCH.h"

#include "GameFramework/Actor.h"

UActorComponent::~UActorComponent()
{
    if (Owner)
    {
        Owner->RemoveOwnedComponent(this);
    }
}

void UActorComponent::RegisterComponent()
{
    if (bRegistered || !Owner || !Owner->GetWorld()) return;
    OnRegister();
    bRegistered = true;
}

void UActorComponent::UnregisterComponent()
{
    if (!bRegistered) return;
    OnUnregister();
    bRegistered = false;
}

void UActorComponent::OnRegister()
{
}

void UActorComponent::OnUnregister()
{
}