#include "EnginePCH.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"

// 스텐실 마스크·외곽선 두 패스의 Shader와 상수 버퍼를 준비한다.
// 두 패스 모두 깊이 테스트를 끄므로 다른 물체에 가려져도 외곽선이 보인다.
void FOutlineRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/OutlineShader.hlsl");

	// 1패스: 원본 크기 메시 영역을 색 없이 스텐실에만 기록한다.
	MaskPipelineState.Shader = Shader;
	MaskPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	MaskPipelineState.RasterizerState = ERasterizerState::SolidNone;
	MaskPipelineState.BlendState = EBlendState::NoColorWrite;
	MaskPipelineState.DepthStencilState = EDepthStencilState::StencilMask;

	// 2패스: 확장 메시를 스텐실이 비어 있는 곳에만 그려 테두리 띠만 남긴다.
	OutlinePipelineState.Shader = Shader;
	OutlinePipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	OutlinePipelineState.RasterizerState = ERasterizerState::SolidNone;
	OutlinePipelineState.BlendState = EBlendState::Opaque;
	OutlinePipelineState.DepthStencilState = EDepthStencilState::StencilOutline;

	static_assert(sizeof(FOutlineData) == 208, "Outline constant buffer layout mismatch");
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FOutlineData));
}

// 외부에서 지정한 Mesh 참조를 저장한다.
void FOutlineRenderer::SetMesh(UStaticMesh* InMesh)
{
	Mesh = InMesh;
}

// 카메라 거리 대신 View별 픽셀 크기와 고정 확장량으로 선택 Outline을 그린다.
void FOutlineRenderer::OnRender(const FOutline& InOutline, const FMatrix& InViewProj, const FViewportSettings& Viewport)
{
	if (!InOutline.HasSelection() || Viewport.Width == 0 || Viewport.Height == 0) return;

	const TArray<UPrimitiveComponent*> Targets = InOutline.GetTargets();
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	const float Width = static_cast<float>(Viewport.Width);
	const float Height = static_cast<float>(Viewport.Height);

	// 모든 선택 메시의 마스크를 먼저 합쳐 자식 메시 사이에 불필요한 경계가 생기지 않게 한다.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		RenderCommand::BindPipelineState(Pass == 0 ? MaskPipelineState : OutlinePipelineState);
		for (UPrimitiveComponent* Target : Targets)
		{
			UStaticMesh* TargetMesh = Target->GetRenderMesh();
			if (!Target->IsRegistered() || !Target->IsVisible() || !TargetMesh || !TargetMesh->IndexBuffer) continue;

			const FMatrix World = Target->GetWorldMatrix();
			const FMatrix NormalMatrix = World.Inverse().GetTransposed();
			FOutlineData Constants{World, NormalMatrix, InViewProj,
				FVector4(Width, Height, Pass == 0 ? 0.0f : 2.0f, 0.0f)};

			RenderCommand::BindMesh(TargetMesh);
			RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants, sizeof(Constants));
			RenderCommand::DrawIndexed(TargetMesh->IndexBuffer->GetIndexCount());
		}
	}

	// Gizmo와 이후 패스에 선택용 상태가 남지 않도록 복원한다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
}
