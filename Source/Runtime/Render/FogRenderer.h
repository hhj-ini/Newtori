#pragma once

#include "Buffer.h"
#include "PipelineState.h"
#include "Texture2D.h"

class FRenderDevice;

// HLSL의 cbuffer FogConstants와 레이아웃이 일치해야 한다.
struct FHeightFogConstants
{
	FVector4 FogInscatteringColor;
	float FogHeight;
	float FogDensity;
	float FogHeightFalloff;
	float StartDistance;
	float FogCutoffDistance;
	float FogMaxOpacity;
	float padding[2];
};

// 안개를 화면 전체에 그린다. 깊이를 쓰므로 불투명 물체보다 나중에 호출해야 한다.
// 정점 버퍼 없이 전체 화면 삼각형 하나만 그리므로 메시가 필요 없다....?
class FFogRenderer
{
public:
	FFogRenderer() = default;
	~FFogRenderer() = default;
	bool Init(FRenderDevice* InRenderDevice);

	// 현재 RTV에 합성한다. 바인딩된 DSV를 복사해 읽고 호출 전 상태를 복원한다.
	void OnRender(const FMatrix& ViewProjection, const FVector& CameraPosition, const FHeightFogConstants& FogConstants);

private:
	FRenderDevice* RenderDevice = nullptr; // EngineLoop owns this device.
	struct FFogResources
	{
		ComPtr<ID3D11VertexShader> VS;
		ComPtr<ID3D11PixelShader> PS;
		ComPtr<ID3D11Buffer> ViewBuffer, FogBuffer;
		ComPtr<ID3D11BlendState> Blend;
		ComPtr<ID3D11Texture2D> DepthCopy;
		ComPtr<ID3D11ShaderResourceView> DepthSRV;
		UINT Width = 0, Height = 0;
		bool AttemptedInit = false;
		bool ReportedDraw = false;
		bool ReportedMissingDepth = false;
	} Resources;
};
