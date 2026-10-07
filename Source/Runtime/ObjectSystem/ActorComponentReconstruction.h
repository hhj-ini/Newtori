#pragma once

#include "GameFramework/Actor.h"
#include "Engine/World.h"

struct FActorComponentDescriptor
{
	FName Name;
	UClass* Class;
};

// 저장/복제된 구성에 맞추되 생성자의 기본 컴포넌트는 재사용한다.
// 이름이 바뀐 구버전 파일도 같은 클래스의 기본 컴포넌트로 복원할 수 있다.
inline TArray<UActorComponent*> ReconstructActorComponents(
	AActor* Actor, const TArray<FActorComponentDescriptor>& Descriptors)
{
	const TArray<UActorComponent*> Defaults = Actor->GetComponents();
	TArray<UActorComponent*> Result;
	TArray<UActorComponent*> Used;

	// 이름/클래스가 모두 일치하는 기본 컴포넌트를 먼저 확보한다.
	for (const FActorComponentDescriptor& Descriptor : Descriptors)
	{
		UActorComponent* Match = nullptr;
		for (UActorComponent* Component : Defaults)
		{
			if (Used.Find(Component) == INDEX_NONE && Component->GetClass() == Descriptor.Class &&
				Component->GetFName() == Descriptor.Name)
			{
				Match = Component;
				Used.Add(Component);
				break;
			}
		}
		Result.Add(Match);
	}

	// 이름 변경은 같은 클래스의 기본 객체를 재사용하고, 추가된 컴포넌트만 생성한다.
	for (int32 Index = 0; Index < Descriptors.Num(); ++Index)
	{
		if (!Result[Index])
		{
			for (UActorComponent* Component : Defaults)
			{
				if (Used.Find(Component) == INDEX_NONE && Component->GetClass() == Descriptors[Index].Class)
				{
					Result[Index] = Component;
					Used.Add(Component);
					break;
				}
			}
		}
		if (!Result[Index])
		{
			Result[Index] = Cast<UActorComponent>(FObjectFactory::ConstructObject(Descriptors[Index].Class, Actor));
			Actor->AddOwnedComponent(Result[Index]);
		}
		if (Result[Index])
			Result[Index]->SetName(Descriptors[Index].Name);
	}

	// 삭제할 기본 컴포넌트가 자식을 승격하거나 위치를 바꾸지 않도록 먼저 분리한다.
	for (UActorComponent* Component : Actor->GetComponents())
		if (USceneComponent* Scene = Cast<USceneComponent>(Component))
			Scene->DetachFromParent();
	Actor->SetRootComponent(nullptr);

	for (UActorComponent* Component : Defaults)
	{
		if (Used.Find(Component) != INDEX_NONE)
			continue;
		Component->DestroyComponent();
	}

	return Result;
}

inline void RegisterRestoredActorComponents(AActor* Actor)
{
	for (UActorComponent* Component : Actor->GetComponents())
	{
		Component->RegisterComponent();
		if (USceneComponent* Scene = Cast<USceneComponent>(Component))
			Scene->MarkTransformDirty();
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			Primitive->MarkRenderStateDirty();
	}

	// Editor의 게임 Tick 등록은 Actor의 허용 상태에 맞춘다.
	if (!Actor->GetWorld()->IsGameWorld())
		Actor->RegisterAllActorTickFunctions(Actor->bTickInEditor);
}
