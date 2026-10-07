#include "EnginePCH.h"
#include "MovementComponent.h"
#include "SceneComponent.h"
#include "GameFramework/Actor.h"

void UMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (!UpdatedComponent)
	{
		if (AActor* MyActor = GetOwner())
		{
			if (USceneComponent* NewUpdatedComponent = MyActor->GetRootComponent())
			{
				SetUpdatedComponent(NewUpdatedComponent);
			}
		}
	}
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	UpdatedComponent = NewUpdatedComponent;
}

void UMovementComponent::MoveUpdateComponent(const FVector& NewLocation, const FQuat& NewRotation)
{
	if (UpdatedComponent)
	{
		UpdatedComponent->SetRelativeLocation(NewLocation);
		UpdatedComponent->SetRelativeRotationFromQuat(NewRotation);
	}
}