#pragma once

#include "../Math/Transform.h"
#include "ActorComponent.h"
#include "Math/Box.h"

class USceneComponent : public UActorComponent
{
	DECLARE_CLASS(USceneComponent, UActorComponent)

	REFLECT_START(ClassName)
		PROPERTY(Transform)
	REFLECT_END()

public:
	USceneComponent() = default;
	virtual ~USceneComponent() override;

	// Get & Set
	const FVector& GetRelativeLocation() const { return Transform.Location; }
	void SetRelativeLocation(const FVector& InLocation) 
	{
		Transform.Location = InLocation;
		MarkTransformDirty();
	}

	const FRotator& GetRelativeRotation() const { return Transform.Rotation; }
	void SetRelativeRotation(const FRotator& InRotation) 
	{
		Transform.Rotation = InRotation; 
		MarkTransformDirty();
	}

	void SetRelativeRotationFromQuat(const FQuat& InQuat)
	{
		Transform.Rotation = InQuat.ToFRotator();
		MarkTransformDirty();
	}

	const FVector& GetRelativeScale3D() const { return Transform.Scale; }
	void SetRelativeScale3D(const FVector& InScale) 
	{
		Transform.Scale = InScale;
		MarkTransformDirty();
	}

	// 쿼터니언 적용된 회전행렬
	FQuat GetRelativeRotationQuat() const { return Transform.GetOrientation(); }

	const FTransform& GetTransform() const { return Transform; }
	void SetTransform(const FTransform& InTransform) 
	{
		Transform = InTransform;
		MarkTransformDirty();
	}

	// Attatch-To
	USceneComponent* GetAttachParent() const { return AttachParent; }
	const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }
	void SetupAttachment(USceneComponent* InParent);
	void DetachFromParent();

	virtual FBox CalcLocalBounds() const { return FBox{ FVector(), FVector() }; }
	virtual FBox CalcBounds() const { return CalcLocalBounds().GetWorldAABB(GetWorldMatrix()); }

	FVector GetWorldLocation() const;
	void SetWorldLocation(const FVector& InLocation);
	void SetWorldRotation(const FRotator& InRotation);
	FRotator GetWorldRotation() const;
	FVector GetWorldScale3D() const;
	FMatrix GetWorldMatrix() const;

	void OnPropertyChanged(const FProperty& Property) override;
	void MarkTransformDirty();
	virtual void OnTransformDirty() {};


protected:
	// 부모 기준의 상대 Transform
	bool bTransformDirty;
	FTransform Transform;

	// 부모 SceneComponent와의 연결 관계
	USceneComponent* AttachParent = nullptr;

	// 현재 SceneComponent에 연결된 자식 목록
	TArray<USceneComponent*> AttachChildren;

};