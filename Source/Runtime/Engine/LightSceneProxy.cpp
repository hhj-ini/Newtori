#include "EnginePCH.h"
#include "LightSceneProxy.h"
#include "Component/PointLightComponent.h"


FLightSceneProxy::FLightSceneProxy(const ULightComponent* LightComponent)
{
	UpdateFromComponent(*LightComponent);
}

void FLightSceneProxy::UpdateFromComponent(const ULightComponent& Component)
{
    LightType = Component.GetLightType();
    Position = Component.GetWorldLocation();

    Component.GetLightColor(LightColor);
    Component.GetIntensity(Intensity);

    AttenuationRadius = 0.0f;

    if (LightType == ELightType::Point)
    {

        const auto& Point = static_cast<const UPointLightComponent&>(Component);

        AttenuationRadius = Point.GetAttenuationRadius();
    }
}
