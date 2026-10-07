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

void UActorComponent::DestroyComponent()
{
    // 파생 Component가 살아 있을 때 OnUnregister를 호출해
    // Scene/Tick 등 외부 시스템의 참조를 먼저 제거한다.
    UnregisterComponent();

    delete this;
}

void UActorComponent::OnRegister()
{
}

void UActorComponent::OnUnregister()
{
}