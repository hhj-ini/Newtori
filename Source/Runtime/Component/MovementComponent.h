#pragma once
#include "ActorComponent.h"

class UPrimitiveComponent;
class USceneComponent;

class UMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UMovementComponent, UActorComponent)

public:
	UMovementComponent() {}
	virtual ~UMovementComponent() override = default;

	virtual void TickComponent(float DeltaTime) {};

	virtual void InitializeComponent() override;

	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
protected:
	// 대상이 삭제되거나 루트가 바뀌었을 때 해제된 포인터로 Tick하지 않는다.
	bool ResolveUpdatedComponent();

	void MoveUpdateComponent(const FVector& DeltaLocation, const FQuat& NewRotation);

	FVector Velocity;
	
	// 업데이트 되는 대상이 메시/물리 아닐경우
	USceneComponent* UpdatedComponent = nullptr;

	// 업데이트 되는 대상이 메시/물리인 경우
	UPrimitiveComponent* UpdatedPrimitive = nullptr;
};
