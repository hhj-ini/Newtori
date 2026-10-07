#include "EnginePCH.h"
#include "Component/SceneComponent.h"

#include "GameFramework/Actor.h"
#include "ObjectSystem/Property.h"

USceneComponent::~USceneComponent()
{
	TArray<USceneComponent*> Children = AttachChildren;

	TArray<FMatrix> ChildWorldMatrices;
	for (USceneComponent* Child : Children)
	{
		ChildWorldMatrices.Add(Child->GetWorldMatrix());
	}
	AttachChildren.Reset();

	AActor* OwnerActor = GetOwner();
	const bool bIsRoot = OwnerActor && OwnerActor->GetRootComponent() == this;

	USceneComponent* NewRoot = nullptr;

	if (bIsRoot && !Children.IsEmpty())
	{
		NewRoot = Children[0];
	}

	if (bIsRoot && NewRoot)
	{
		// 첫 번째 자식을 새 Root로 승격한다.
		NewRoot->AttachParent = nullptr;
		NewRoot->SetTransform(FTransform::FromMatrix(ChildWorldMatrices[0]));

		OwnerActor->SetRootComponent(NewRoot);

		// 나머지 자식들은 새 Root 아래로 옮기되 World Transform은 유지한다.
		for (uint32 i = 1; i < Children.Num(); ++i)
		{
			USceneComponent* Child = Children[i];

			FMatrix NewRootWorld = NewRoot->GetWorldMatrix();
			FMatrix NewLocalMatrix = ChildWorldMatrices[i] * NewRootWorld.Inverse();

			Child->AttachParent = nullptr;
			Child->SetupAttachment(NewRoot);
			Child->SetTransform(FTransform::FromMatrix(NewLocalMatrix));
		}
	}
	else
	{
		USceneComponent* NewParent = AttachParent;

		for (uint32 i = 0; i < Children.Num(); ++i)
		{
			USceneComponent* Child = Children[i];

			FMatrix NewLocalMatrix = ChildWorldMatrices[i];

			if (NewParent)
			{
				FMatrix NewParentWorld = NewParent->GetWorldMatrix();
				NewLocalMatrix = ChildWorldMatrices[i] * NewParentWorld.Inverse();
			}

			Child->AttachParent = nullptr;
			Child->SetupAttachment(NewParent);
			Child->SetTransform(FTransform::FromMatrix(NewLocalMatrix));
		}
	}

	DetachFromParent();
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
	if (InParent == this || AttachParent == InParent) return;

	// 순환 체크
	for (USceneComponent* Parent = InParent; Parent != nullptr; Parent = Parent->AttachParent)
		if (Parent == this) return;

	DetachFromParent();
	AttachParent = InParent;
	if (AttachParent)
	{
		AttachParent->AttachChildren.Add(this);
	}
	MarkTransformDirty();
}

void USceneComponent::DetachFromParent()
{
	if (!AttachParent) return;

	TArray<USceneComponent*>& Siblings = AttachParent->AttachChildren;
	for (uint32 i = 0;i < Siblings.Num(); ++i)
	{
		if (Siblings[i] == this)
		{
			Siblings.RemoveAt(i, 1);
			break;
		}
	}
	AttachParent = nullptr;
	MarkTransformDirty();
}

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetWorldRotation().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return Transform.Rotation;
}

void USceneComponent::SetWorldLocation(const FVector& InLocation)
{
	const FVector Local = AttachParent ? AttachParent->GetWorldMatrix().Inverse().TransformPosition(InLocation) : InLocation;
	SetRelativeLocation(Local);
}

void USceneComponent::SetWorldRotation(const FRotator& InRotation)
{
	const FQuat WorldRotation = InRotation.Quaternion();
	const FQuat Local = AttachParent ? AttachParent->GetWorldRotation().Quaternion().Inverse() * WorldRotation : WorldRotation;
	SetRelativeRotation(Local.ToFRotator());
}

FVector USceneComponent::GetWorldLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
}

FVector USceneComponent::GetWorldScale3D() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetWorldScale3D();

		return FVector(
			Transform.Scale.X * ParentScale.X,
			Transform.Scale.Y * ParentScale.Y,
			Transform.Scale.Z * ParentScale.Z
		);
	}

	return Transform.Scale;
}

FMatrix USceneComponent::GetWorldMatrix() const
{
	FMatrix LocalMatrix = Transform.GetLocalMatrix(); // 부모 컴포넌트 연결 없을 때

	if (AttachParent)
	{
		return LocalMatrix * AttachParent->GetWorldMatrix();
	}

	return LocalMatrix;
}

void USceneComponent::OnPropertyChanged(const FProperty& Property)
{
	Super::OnPropertyChanged(Property);

	if (Property.Name == "Transform")
	{
		MarkTransformDirty();
	}
}

void USceneComponent::MarkTransformDirty()
{
	OnTransformDirty();         

	for (USceneComponent* Child : AttachChildren)
		Child->MarkTransformDirty();      // 부모가 움직이면 자식의 월드 행렬도 바뀐다
}


