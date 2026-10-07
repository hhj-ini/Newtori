#pragma once

#include "ObjectSystem/Object.h"
#include "Component/PrimitiveComponent.h"

#include "ObjectSystem/Class.h"
#include "ObjectSystem/ObjectFactory.h"
#include "../Container/Set.h"
#include "Engine/EngineBaseTypes.h"

class UWorld;
class ULevel;

class UTextRenderComponent;

class AActor : public UObject
{
	DECLARE_CLASS(AActor, UObject)
	REFLECT_START(ClassName)
		PROPERTY(bTickInEditor)
	REFLECT_END()
public:
	AActor();
	virtual ~AActor();

	virtual void BeginPlay();
	virtual void EndPlay();
	bool HasBegunPlay() const { return bHasBegunPlay; }
	void OnPropertyChanged(const FProperty& Property) override;
	// Editor Tick 허용 여부이며 BeginPlay를 시작하지는 않는다. bCanEverTick 조건은 별도로 적용한다.
	bool bTickInEditor = false;

	// 액터 자신의 로직. 컴포넌트는 각자의 PrimaryComponentTick으로 따로 실행된다.
	virtual void Tick(float DeltaTime) {}

	// FActorTickFunction이 호출하는 진입점
	void TickActor(float DeltaTime) { Tick(DeltaTime); }

	UWorld* GetWorld() const { return World; }
	ULevel* GetLevel() const { return Level; }

	const TArray<UActorComponent*>& GetComponents() const { return Components; }

	// 컴포넌트 추가/삭제 뒤에도 현재 소유 목록을 기준으로 조회한다.
	template <typename T>
	T* FindComponentByClass() const
	{
		for (UActorComponent* Component : Components)
		{
			if (T* Match = Cast<T>(Component)) return Match;
		}
		return nullptr;
	}


	USceneComponent* GetRootComponent() const { return RootComponent; }
	void SetRootComponent(USceneComponent* SceneComponent) { RootComponent = SceneComponent; }

	UActorComponent* AddComponentByClass(UClass* Class, USceneComponent* AttachParent = nullptr);
	void AddOwnedComponent(UActorComponent* Component);
	void RemoveOwnedComponent(UActorComponent* Component);
	// 에디터 표시용 텍스트도 Actor가 소유하되 저장/PIE 복제에는 포함하지 않는다.
	UTextRenderComponent* GetUUIDTextComponent();

	FVector GetActorLocation() const;
	FRotator GetActorRotation() const;
	FVector GetActorScale3D() const;
	FQuat GetActorQuat() const;
	FTransform GetActorTransform() const;

	bool Destroy();

	friend class UWorld;

	template <typename T>
	T* CreateDefaultSubobject(FName Name)
	{
		T* Component = CastChecked<T>(FObjectFactory::ConstructObject(T::StaticClass(), this, Name));
		Component->SetOwner(this);
		Components.Add(Component);
		return Component;
	}

	// bCanEverTick이 켜진 액터·컴포넌트의 Tick 함수만 World의 FTickTaskManager에 등록하거나 해제한다.
	void RegisterAllActorTickFunctions(bool bRegister);

	// UE와 같이 기본값은 bCanEverTick = false. 생성자에서 Target = this
	FActorTickFunction PrimaryActorTick;

protected:
	TArray<UActorComponent*> Components;
	USceneComponent* RootComponent = nullptr;

	UWorld* World = nullptr;
	ULevel* Level = nullptr;

private:
	bool bHasBegunPlay = false;
	UTextRenderComponent* UUIDTextComponent = nullptr;
};
