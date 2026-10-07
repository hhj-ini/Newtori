#pragma once

#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

// Actor 선택은 소유 메시 전체, Component 선택은 해당 메시만 강조한다.
class FOutline
{
public:
	void SetActor(AActor* InActor) { Actor = InActor; Target = nullptr; }
	void SetTarget(UPrimitiveComponent* InTarget) { Actor = nullptr; Target = InTarget; }
	bool HasSelection() const { return Actor || Target; }
	AActor* GetActor() const { return Actor ? Actor : Target ? Target->GetOwner() : nullptr; }

	TArray<UPrimitiveComponent*> GetTargets() const
	{
		TArray<UPrimitiveComponent*> Targets;
		if (Target)
		{
			Targets.Add(Target);
		}
		else if (Actor)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
					Targets.Add(Primitive);
			}
		}
		return Targets;
	}

private:
	AActor* Actor = nullptr;
	UPrimitiveComponent* Target = nullptr;
};
