#include "EnginePCH.h"
#include "MovementComponent.h"
#include "SceneComponent.h"
#include "PrimitiveComponent.h"
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
	if (NewUpdatedComponent && NewUpdatedComponent->GetOwner() != GetOwner())
		return;
	UpdatedComponent = NewUpdatedComponent;
	UpdatedPrimitive = Cast<UPrimitiveComponent>(NewUpdatedComponent);
}

bool UMovementComponent::ResolveUpdatedComponent()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
		return false;

	// 소유 목록에서 생존 여부를 먼저 확인한다. 삭제된 대상은 역참조하지 않는다.
	if (!UpdatedComponent || OwnerActor->GetComponents().Find(UpdatedComponent) == INDEX_NONE)
		SetUpdatedComponent(OwnerActor->GetRootComponent());
	return UpdatedComponent != nullptr;
}

void UMovementComponent::MoveUpdateComponent(const FVector& DeltaLocation, const FQuat& NewRotation)
{
	if (UpdatedComponent)
	{
		UpdatedComponent->SetRelativeLocation(DeltaLocation + UpdatedComponent->GetRelativeLocation());
		UpdatedComponent->SetRelativeRotationFromQuat(NewRotation);
	}
}
