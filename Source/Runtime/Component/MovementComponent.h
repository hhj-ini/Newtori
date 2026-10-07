#pragma once
#include "ActorComponent.h"

class UPrimitiveComponent;
class USceneComponent;

class UMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UMovementComponent, UActorComponent)

public:
	UMovementComponent() {}
	virtual ~UMovementComponent() override;

	virtual void BeginPlay() {};
	virtual void TickComponent(float DeltaTime) {};

	virtual void InitializeComponent() override;

	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	void MoveUpdateComponent(const FVector& NewLocation, const FQuat& NewRotation);

	FVector Velocity;
	
	// 업데이트 되는 대상이 메시/물리 아닐경우
	USceneComponent* UpdatedComponent;

	// 업데이트 되는 대상이 메시/물리인 경우
	UPrimitiveComponent* UpdatedPrimitive;
};