#include "EnginePCH.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UActorComponent::BeginPlay()
{
	bHasBegunPlay = true;
}

void UActorComponent::EndPlay()
{
	PrimaryComponentTick.UnRegisterTickFunction();
	bHasBegunPlay = false;
}

UActorComponent::~UActorComponent()
{
	PrimaryComponentTick.UnRegisterTickFunction();
    if (Owner)
    {
        Owner->RemoveOwnedComponent(this);
    }
}

void UActorComponent::RegisterComponent()
{
    if (bRegistered || !Owner || !Owner->GetWorld()) return;
	bRegistered = true;
    OnRegister();
	InitializeComponent();
	if (Owner->HasBegunPlay() && !HasBegunPlay())
	{
		BeginPlay();
	}
	if (Owner->HasBegunPlay() || (Owner->GetWorld()->GetWorldType() == EWorldType::Editor && Owner->bTickInEditor))
	{
		PrimaryComponentTick.RegisterTickFunction(Owner->GetWorld()->GetTickTaskManager());
	}

}

void UActorComponent::UnregisterComponent()
{
	PrimaryComponentTick.UnRegisterTickFunction();
    if (!bRegistered) return;
    OnUnregister();
    bRegistered = false;
}

void UActorComponent::DestroyComponent()
{
	if (HasBegunPlay())
		EndPlay();
    // 파생 Component가 살아 있을 때 OnUnregister를 호출해
    // Scene/Tick 등 외부 시스템의 참조를 먼저 제거한다.
    UnregisterComponent();

    delete this;
}

void UActorComponent::InitializeComponent()
{
    if (!bRegistered) return;
    if (bHasBeenInitialized) return;

    bHasBeenInitialized = true;
}

void UActorComponent::OnRegister()
{
}

void UActorComponent::OnUnregister()
{
}
