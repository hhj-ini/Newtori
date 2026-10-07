#pragma once
#include "Component/LocalLightComponent.h"

class UPointLightComponent : public ULocalLightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULocalLightComponent)

public:
	ELightType GetLightType() const { return ELightType::Point; }

};
