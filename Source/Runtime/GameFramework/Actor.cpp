#include "EnginePCH.h"
#include "Component/TextRenderComponent.h"
#include "Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Component/SceneComponent.h"

AActor::AActor()
{
    PrimaryActorTick.Target = this;
}

AActor::~AActor()
{
	EndPlay();
	RegisterAllActorTickFunctions(false);
    TArray<UActorComponent*> ToDelete = Components;
    Components.Reset();
    RootComponent = nullptr;

    for (UActorComponent* Component : ToDelete)
    {
		Component->DestroyComponent();
    }
}

void AActor::BeginPlay()
{
	if (bHasBegunPlay)
		return;
	bHasBegunPlay = true;

	for (UActorComponent* Component : Components)
	{
		if (Component->IsRegistered() && !Component->HasBegunPlay())
		Component->BeginPlay();
	}

	RegisterAllActorTickFunctions(true);
}

void AActor::EndPlay()
{
	RegisterAllActorTickFunctions(false);
	if (!bHasBegunPlay)
		return;
	bHasBegunPlay = false;
	for (UActorComponent* Component : Components)
	{
		if (Component->HasBegunPlay())
			Component->EndPlay();
	}
}

void AActor::OnPropertyChanged(const FProperty& Property)
{
	Super::OnPropertyChanged(Property);
	if (Property.Name == "bTickInEditor" && World && !World->IsGameWorld())
	{
		RegisterAllActorTickFunctions(bTickInEditor);
	}
}

void AActor::RegisterAllActorTickFunctions(bool bRegister)
{
	if (bRegister && !World)
		return;

	// bCanEverTick이 꺼진 함수는 등록하지 않으므로 정적 메시 액터는 매 프레임 순회 대상에서 빠진다.
	auto Apply = [&](FTickFunction& Function)
	{
		if (bRegister)
			Function.RegisterTickFunction(World->GetTickTaskManager());
		else
			Function.UnRegisterTickFunction();
	};

	Apply(PrimaryActorTick);
	for (UActorComponent* Component : Components)
	{
		if (Component && (!bRegister || Component->IsRegistered()))
			Apply(Component->PrimaryComponentTick);
	}
}

UActorComponent* AActor::AddComponentByClass(UClass* Class, USceneComponent* AttachParent)
{
	if (!Class || !Class->Constructor || !Class->IsChildOf(UActorComponent::StaticClass())) return nullptr;
	if (AttachParent && AttachParent->GetOwner() != this) return nullptr;

	// 생성과 소유권 설정을 먼저 완료한다.
	UActorComponent* Component = Cast<UActorComponent>(FObjectFactory::ConstructObject(Class, this));
	if (!Component) return nullptr;
	AddOwnedComponent(Component);

	// 최종 부모를 등록 전에 정한다. Particle 등 OnRegister/BeginPlay가 위치를 읽기 때문이다.
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		if (RootComponent)
		{
			SceneComponent->SetupAttachment(AttachParent ? AttachParent : RootComponent);
		}
		else
		{
			SetRootComponent(SceneComponent);
		}
	}

	// 등록은 Scene과 재생 중인 World의 생명주기 연결까지 담당한다.
	Component->RegisterComponent();
	return Component;
}

void AActor::AddOwnedComponent(UActorComponent* Component)
{
    if (!Component) return;
    for (UActorComponent* CurrentComponent : Components)
    {
        if (Component == CurrentComponent)
        {
            return;
        }
    }
    Component->SetOwner(this);
    Components.Add(Component);
}

UTextRenderComponent* AActor::GetUUIDTextComponent()
{
	if (!UUIDTextComponent)
	{
		UUIDTextComponent = Cast<UTextRenderComponent>(
			FObjectFactory::ConstructObject(UTextRenderComponent::StaticClass(), this, FName("ActorUUID")));
		UUIDTextComponent->SetFlags(EObjectFlags::RF_Transient);
		AddOwnedComponent(UUIDTextComponent);
		UUIDTextComponent->SetTextSize(0.5f);
	}
	UUIDTextComponent->SetText("UUID : " + std::to_string(GetUUID()));
	return UUIDTextComponent;
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	if (Component == UUIDTextComponent)
		UUIDTextComponent = nullptr;
    Component->UnregisterComponent();

    for (uint32 i = 0; i < Components.Num(); ++i)
    {
        if (Components[i] == Component)
        {
            Components.RemoveAt(i, 1);
            break;
        }
    }

    if (RootComponent == Component)
    {
        RootComponent = nullptr;
    }
}

FVector AActor::GetActorLocation() const
{
    if (RootComponent)
    {
        return RootComponent->GetWorldLocation();
    }
    return FVector::ZeroVector;
}

FVector AActor::GetActorScale3D() const
{
    if (RootComponent)
    {
        return RootComponent->GetWorldScale3D();
    }
    return FVector::OneVector;
}

FTransform AActor::GetActorTransform() const
{
    if (RootComponent)
    {
        return FTransform(
            RootComponent->GetWorldRotation(),
            RootComponent->GetWorldLocation(),
            RootComponent->GetWorldScale3D()
        );
    }
    return FTransform::Identity;
}

bool AActor::Destroy()
{
    if (!World)
        return false;

    return World->DestroyActor(this);
}
