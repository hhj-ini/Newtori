#include "EnginePCH.h"
#include "RotatingMovementComponent.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (!ResolveUpdatedComponent())
		return;

	// Compute new rotation
	const FQuat OldRotation = UpdatedComponent->GetRelativeRotationQuat();
	const FQuat DeltaRotation = (RotationRate * DeltaTime).Quaternion();
	const FQuat NewRotation = bRotationInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation);

	// Compute new location
	FVector DeltaLocation = FVector::ZeroVector;
	if (!PivotTranslation.IsZero())
	{
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = (OldPivot - NewPivot); // ConstrainDirectionToPlane() not necessary because it's done by MoveUpdatedComponent() below.
	}

	MoveUpdateComponent(DeltaLocation, NewRotation);
}
