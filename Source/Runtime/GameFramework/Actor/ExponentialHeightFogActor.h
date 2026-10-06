#pragma once
#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/ExponentialHeightFogComponent.h"


class AExponentialHeightFogActor : public AActor
{
	DECLARE_CLASS(AExponentialHeightFogActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	AExponentialHeightFogActor(); 
	virtual ~AExponentialHeightFogActor() override = default; ;

private:
	// 클릭해서 고를 수 있어야 하므로 프리미티브인 빌보드를 루트로 둔다
	UBillboardComponent* BillboardComponent = nullptr;

	UExponentialHeightFogComponent* ExponentialHeightFogComponent = nullptr;

};
