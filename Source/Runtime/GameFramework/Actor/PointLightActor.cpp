#include "EnginePCH.h"
#include "GameFramework/Actor/PointLightActor.h"
#include <Asset/AssetManager.h>

APointLightActor::APointLightActor()
{
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");

    SetRootComponent(MeshComponent);


    // 메시를 먼저 지정한다.
    if (UStaticMesh* Sphere = UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"))
    {
        MeshComponent->SetStaticMesh(Sphere);

        // 이 구 컴포넌트에서만 사용할 머티리얼을 복제한다.
        UMaterial* MaterialInstance = UMaterial::CreateInstance(MeshComponent->GetMaterial(0));
		MaterialInstance->BaseColor = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

        if (MaterialInstance)
        {
            MaterialInstance->bUnlit = true;

            // 공유 Sphere 에셋 대신 컴포넌트의 슬롯을 덮어쓴다.
            MeshComponent->SetMaterial(0, MaterialInstance);
        }
    }

    PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");
    PointLightComponent->SetLightColor(FVector4(1.f, 0.f, 0.f, 1.f));

    PointLightComponent->SetupAttachment(MeshComponent);
}   