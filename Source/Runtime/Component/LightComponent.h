#pragma once

#include <EnginePCH.h>
#include "SceneComponent.h"
#include "../Engine/LightEnums.h"

class FLightSceneProxy;
class FScene;

class ULightComponent : public USceneComponent
{
	DECLARE_CLASS(ULightComponent, USceneComponent)
	REFLECT_START(ULightComponent)
		PROPERTY(Intensity)
		PROPERTY_TYPE(LightColor, Color)
	REFLECT_END()
public:
    virtual ELightType GetLightType() const = 0;

    FLightSceneProxy* GetSceneProxy() const { return SceneProxy; }

    void GetIntensity(float& OutIntensity) const{OutIntensity = Intensity;}

    void GetLightColor(FVector4& OutColor) const{ OutColor = LightColor;}

    void SetIntensity(float InIntensity);
    void SetLightColor(const FVector4& InColor);

    void Serialize(json& Handle, bool bIsLoading) override;
    void OnPropertyChanged(const FProperty& Property) override;
    void OnTransformDirty() override;

    void MarkLightDirty();

protected:
    virtual void ClampLightValues();

private:
    friend class FScene;

    void SetSceneProxy(FLightSceneProxy* InProxy){ SceneProxy = InProxy; }

    float Intensity = 1.0f;
    FVector4 LightColor = FVector4(1, 1, 1, 1);

    // FScene이 소유한다.
    FLightSceneProxy* SceneProxy = nullptr;
};