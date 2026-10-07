#pragma once

#include "Component/SceneComponent.h"

class FScene;

class UExponentialHeightFogComponent : public USceneComponent
{
	DECLARE_CLASS(UExponentialHeightFogComponent, USceneComponent)
	REFLECT_START(UExponentialHeightFogComponent)
		PROPERTY(FogDensity)
		PROPERTY(FogHeightFalloff)
		PROPERTY(StartDistance)
		PROPERTY(FogCutoffDistance)
		PROPERTY(FogMaxOpacity)
		PROPERTY_TYPE(FogInscatteringColor,Color)
	REFLECT_END()

public:
	UExponentialHeightFogComponent() = default;
	virtual ~UExponentialHeightFogComponent() override = default;

	UExponentialHeightFogComponent(const UExponentialHeightFogComponent&) = delete;
	UExponentialHeightFogComponent& operator=(const UExponentialHeightFogComponent&) = delete;

	float GetFogDensity() const { return FogDensity; }
	void SetFogDensity(float InFogDensity) { FogDensity = InFogDensity; ClampFogValues(); MarkFogDirty(); }

	float GetFogHeightFalloff() const { return FogHeightFalloff; }
	void SetFogHeightFalloff(float InFogHeightFalloff) { FogHeightFalloff = InFogHeightFalloff; ClampFogValues(); MarkFogDirty(); }

	float GetStartDistance() const { return StartDistance; }
	void SetStartDistance(float InStartDistance) { StartDistance = InStartDistance; ClampFogValues(); MarkFogDirty(); }

	float GetFogCutoffDistance() const { return FogCutoffDistance; }
	void SetFogCutoffDistance(float InFogCutoffDistance) { FogCutoffDistance = InFogCutoffDistance; ClampFogValues(); MarkFogDirty(); }

	float GetFogMaxOpacity() const { return FogMaxOpacity; }
	void SetFogMaxOpacity(float InFogMaxOpacity) { FogMaxOpacity = InFogMaxOpacity; ClampFogValues(); MarkFogDirty(); }

	const FVector4& GetFogInscatteringColor() const { return FogInscatteringColor; }
	void SetFogInscatteringColor(const FVector4& InFogInscatteringColor) { FogInscatteringColor = InFogInscatteringColor;  MarkFogDirty();
	}

	void OnTransformDirty() override;
	void MarkFogDirty();
	void OnPropertyChanged(const FProperty& Property) override;
	void Serialize(json& Handle, bool bIsLoading) override;


private:
	void OnRegister() override;
	void OnUnregister() override;
	void ClampFogValues();

	// 안개의 기본 농도
	float FogDensity = 0.3f;
	// 높이가 증가함에 따라 안개가 얼마나 빠르게 희미해지는지 제어하는 계수
	float FogHeightFalloff = 0.2f;
	// 카메라에서 안개가 시작되는 거리
	float StartDistance = 0.0f;
	// 카메라에서 안개가 완전히 사라지는 거리
	float FogCutoffDistance = 0.0f;
	// 안개의 최대 불투명도
	// 안개가 화면을 가릴 수 있는 최대 비율
	float FogMaxOpacity = 1.0f;

	// 안개의 산란 색상 (기본 색상)
	FVector4 FogInscatteringColor = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

};
