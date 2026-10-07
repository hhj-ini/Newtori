#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"
#include "Engine/EngineBaseTypes.h"

class AActor;

class UActorComponent : public UObject
{
	DECLARE_CLASS(UActorComponent, UObject)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	UActorComponent() { PrimaryComponentTick.Target = this; }
	virtual ~UActorComponent() override;

	virtual void BeginPlay() {};
	virtual void TickComponent(float DeltaTime) {};

	void RegisterComponent();
	void UnregisterComponent();
	bool IsRegistered() const { return bRegistered; }

	void DestroyComponent();

	void SetOwner(AActor* InOwner) { Owner = InOwner; }
    AActor* GetOwner() const { return Owner; }

	virtual void InitializeComponent();

	// UE와 같이 기본값은 bCanEverTick = false. Tick이 필요한 컴포넌트만 생성자에서 켠다.
	FActorComponentTickFunction PrimaryComponentTick;

protected:
	virtual void OnRegister();
	virtual void OnUnregister();

private:
	AActor* Owner = nullptr;
	bool bRegistered = false;

	bool bHasBeenInitialized = false;
};