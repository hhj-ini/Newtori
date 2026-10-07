// PointLightActor.cpp
#include "EnginePCH.h"
#include "GameFramework/Actor/PointLightActor.h"

APointLightActor::APointLightActor()
{
    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>( "BillboardComponent");

    SetRootComponent(BillboardComponent);

    PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");

    PointLightComponent->SetupAttachment(BillboardComponent);
}