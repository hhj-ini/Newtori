#pragma once
#include "Component/LightComponent.h"

class ULocalLightComponent : public ULightComponent
{
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)
	REFLECT_START(ULocalLightComponent)
		PROPERTY(AttenuationRadius)
	REFLECT_END()

public:
	float GetAttenuationRadius() const { return AttenuationRadius; }

	void SetAttenuationRadius(float InRadius)
	{
		AttenuationRadius = InRadius;
		ClampLightValues();
		MarkLightDirty();
	}
protected:
	void ClampLightValues() override
	{
		ULightComponent::ClampLightValues();
		if (AttenuationRadius < 0.0f)
			AttenuationRadius = 0.0f;
	}
private:
	float AttenuationRadius = 5.0f;
		
};