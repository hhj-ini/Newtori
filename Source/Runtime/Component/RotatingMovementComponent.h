#pragma once
#include "MovementComponent.h"
#include "Math/Vector.h"

class URotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)
	REFLECT_START(ClassName)
		PROPERTY(RotationRate)
		PROPERTY(PivotTranslation)
		PROPERTY(bRotationInLocalSpace)
		REFLECT_END()
public:
	URotatingMovementComponent() 
	{
		PrimaryComponentTick.bCanEverTick = true;
		RotationRate.Yaw = 180.0f;
		bRotationInLocalSpace = true;
	}
	virtual ~URotatingMovementComponent() override;

	virtual void BeginPlay() {};
	virtual void TickComponent(float DeltaTime) override;

private:
	FRotator RotationRate = FRotator(0.0f, 180.0f, 0.0f);

	FVector PivotTranslation = FVector::ZeroVector;
	uint32 bRotationInLocalSpace;
};