#pragma once
#include "MovementComponent.h"


class URotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

public:
	URotatingMovementComponent() 
	{
		RotationRate.Yaw = 180.0f;
		bRotationInLocalSpace = true;
	}
	virtual ~URotatingMovementComponent() override;

	virtual void BeginPlay() {};
	virtual void TickComponent(float DeltaTime);

private:
	FRotator RotationRate;

	FVector PivotTranslation;
	uint32 bRotationInLocalSpace;
};