#pragma once
#include "Render/MeshRenderData.h"
#include "../Component/LightComponent.h"

// 빛의 위치, 방향, 색상, 세기 등과 같은 정보를 GPU에 전달하기 위한 클래스
// 현재는 1개의 LightSceneProxy만 존재하며, ULightComponent의 SceneProxy로 사용된다.
// SpotLight,PointLight가 같이 사용하기 때문에 나중에 LightType에 따라 다른 구조체를 만들어서 GPU에 전달하는 방식으로 변경해야 한다.
// TODO : FDirectionalLightSceneProxy , FLocalLightSceneProxy 로 분리.
class FLightSceneProxy
{
public:
	explicit FLightSceneProxy(const ULightComponent* LightComponent);
	~FLightSceneProxy() = default;

	FLightSceneProxy(const FLightSceneProxy&) = delete;
	FLightSceneProxy& operator=(const FLightSceneProxy&) = delete;

	
	void UpdateFromComponent(const ULightComponent& LightComponent);


    // 공통
    ELightType LightType = ELightType::Point;
    FVector4 LightColor = FVector4(1, 1, 1, 1);
    float Intensity = 1.0f;

    // Local Light
    FVector Position = FVector(0, 0, 0);
    float AttenuationRadius = 0.0f;

    // Spot Light
    FVector Direction = FVector(1, 0, 0);
    float CosInnerCone = 1.0f;
    float CosOuterCone = 1.0f;
    float ConeFalloffExponent = 1.0f;


};