#pragma once
#include "MovementComponent.h"


class UProjectileMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)
	REFLECT_START(ClassName)
		PROPERTY(InitialSpeed)
		PROPERTY(MaxSpeed)
		REFLECT_END()
public:
	UProjectileMovementComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		InitialSpeed = 1000.0f;
		MaxSpeed = 1000.0f;
	}
	virtual ~UProjectileMovementComponent() override = default;

	virtual void TickComponent(float DeltaTime) override;

	void SetInitialSpeed(float NewSpeed) { InitialSpeed = NewSpeed; }
	void SetMaxSpeed(float NewMaxSpeed) { MaxSpeed = NewMaxSpeed; }
private:
	float InitialSpeed;
	float MaxSpeed;
};
