#include "EnginePCH.h"
#include "ExponentialHeightFogActor.h"
#include "Component/ExponentialHeightFogComponent.h"

AExponentialHeightFogActor::AExponentialHeightFogActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);

	ExponentialHeightFogComponent = CreateDefaultSubobject<UExponentialHeightFogComponent>("UExponentialHeightFogComponent");
	ExponentialHeightFogComponent->SetupAttachment(BillboardComponent);
}