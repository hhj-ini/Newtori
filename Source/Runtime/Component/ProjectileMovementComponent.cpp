#include "EnginePCH.h"
#include "ProjectileMovementComponent.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (!ResolveUpdatedComponent())
		return;

	if (Velocity.IsZero())
	{
		Velocity = UpdatedComponent->GetTransform().GetForward() * InitialSpeed;
	}

	const FVector MoveDelta = Velocity * DeltaTime;

	// 이동 방향을 바라보는 회전값으로 변환
	const FQuat NewRotation = Velocity.ToOrientationQuat();

	MoveUpdateComponent(MoveDelta, NewRotation);
}
