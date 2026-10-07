#pragma once

#include "GameFramework/Actor.h"
#include "Component/StaticMeshComponent.h"
#include "Component/PointLightComponent.h"


// 원래는 ALightActor가 UPointLightComponent를 소유하고 있지만, 
// 편집기에서 액터를 선택했을 때 PointLightComponent의 속성을 바로 보여주기 위해 APointLightActor를 별도로 만든다.
// 또한 FireBallComponent로 인해 pointLight에 구를 붙이는 작업으로 인해 UStaticMeshComponent를 소유하도록 한다.
class APointLightActor : public AActor
{
    DECLARE_CLASS(APointLightActor, AActor)

public:
    APointLightActor();

    UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

private:
	UStaticMeshComponent* MeshComponent = nullptr;
    //UBillboardComponent* BillboardComponent = nullptr;
    UPointLightComponent* PointLightComponent = nullptr;
};