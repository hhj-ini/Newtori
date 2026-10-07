#include "EnginePCH.h"
#include "LightActor.h"
#include "Asset/AssetManager.h"

ALightActor::ALightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);

	BillboardComponent->SetSprite(UAssetManager::GetAssetByPath<UTexture2D>("Assets/Editor/Icon/SpotLight_64x.png"));

	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>("USpotLightComponent");
	SpotLightComponent->SetupAttachment(BillboardComponent);
}
