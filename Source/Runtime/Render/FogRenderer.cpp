#include "EnginePCH.h"
#include "FogRenderer.h"
#include "RenderDevice.h"
#include "RenderUtil.h"

bool FFogRenderer::Init(FRenderDevice* InRenderDevice)
{
    RenderDevice = InRenderDevice;
    return RenderDevice != nullptr;
}

bool FFogRenderer::OnRender(FTexture2D* SceneColor, FTexture2D* SceneDepth, const FMatrix& ViewProjection, const FVector& CameraPosition, const FHeightFogConstants& FogConstants)
{
    if (!RenderDevice || !SceneColor || !SceneDepth ||
        !SceneColor->GetSRV() || !SceneDepth->GetSRV() ||
        FogConstants.FogDensity <= 0.0f)
    {
        return false;
    }

    auto* Device = RenderDevice->GetDevice();
    auto* Context = RenderDevice->GetContext();
    if (!Resources.AttemptedInit)
    {
        Resources.AttemptedInit = true;
        const auto VSCode = RenderUtil::CompileShader("Resources/Shader/ExponentialHeightFogShader.hlsl", "mainVS", EShaderType::Vertex);
        const auto PSCode = RenderUtil::CompileShader("Resources/Shader/ExponentialHeightFogShader.hlsl", "mainPS", EShaderType::Pixel);
        if (!VSCode.IsValid() || !PSCode.IsValid())
        {
            HTR_LOG(Error, "[FogTest] shader compilation failed; restart after fixing the shader.");
            return false;
        }
        D3D11_BUFFER_DESC BufferDesc{};
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        BufferDesc.ByteWidth = sizeof(FMatrix) + 16;
        HRESULT Hr = Device->CreateBuffer(&BufferDesc, nullptr, Resources.ViewBuffer.GetAddressOf());
        BufferDesc.ByteWidth = sizeof(FHeightFogConstants);
        static_assert(sizeof(FHeightFogConstants) == 48);
        if (SUCCEEDED(Hr)) Hr = Device->CreateBuffer(&BufferDesc, nullptr, Resources.FogBuffer.GetAddressOf());
        if (SUCCEEDED(Hr)) Hr = Device->CreateVertexShader(VSCode.GetData(), VSCode.GetSize(), nullptr, Resources.VS.GetAddressOf());
        if (SUCCEEDED(Hr)) Hr = Device->CreatePixelShader(PSCode.GetData(), PSCode.GetSize(), nullptr, Resources.PS.GetAddressOf());
        if (FAILED(Hr))
        {
            HTR_LOG(Error, "[FogTest] resource creation failed: {}", static_cast<uint32>(Hr));
        }
    }
    if (!Resources.VS || !Resources.PS || !Resources.ViewBuffer || !Resources.FogBuffer) return false;

    // Restore pipeline state before the following post-process pass.
    ComPtr<ID3D11RenderTargetView> SavedRTV;
    ComPtr<ID3D11DepthStencilView> SavedDSV;
    ComPtr<ID3D11BlendState> SavedBlend;
    ComPtr<ID3D11DepthStencilState> SavedDepth;
    ComPtr<ID3D11RasterizerState> SavedRaster;
    ComPtr<ID3D11VertexShader> SavedVS;
    ComPtr<ID3D11PixelShader> SavedPS;
    ComPtr<ID3D11GeometryShader> SavedGS;
    ComPtr<ID3D11InputLayout> SavedLayout;
    ComPtr<ID3D11Buffer> SavedCB0, SavedCB1;
    ComPtr<ID3D11ShaderResourceView> SavedSRV0, SavedSRV1;
    FLOAT SavedFactor[4]{};
    UINT SavedMask = 0, SavedStencil = 0;
    D3D11_PRIMITIVE_TOPOLOGY SavedTopology{};

    Context->OMGetRenderTargets(1, SavedRTV.GetAddressOf(), SavedDSV.GetAddressOf());
    Context->OMGetBlendState(SavedBlend.GetAddressOf(), SavedFactor, &SavedMask);
    Context->OMGetDepthStencilState(SavedDepth.GetAddressOf(), &SavedStencil);
    Context->RSGetState(SavedRaster.GetAddressOf());
    Context->VSGetShader(SavedVS.GetAddressOf(), nullptr, nullptr);
    Context->PSGetShader(SavedPS.GetAddressOf(), nullptr, nullptr);
    Context->GSGetShader(SavedGS.GetAddressOf(), nullptr, nullptr);
    Context->IAGetInputLayout(SavedLayout.GetAddressOf());
    Context->IAGetPrimitiveTopology(&SavedTopology);
    Context->PSGetConstantBuffers(0, 1, SavedCB0.GetAddressOf());
    Context->PSGetConstantBuffers(1, 1, SavedCB1.GetAddressOf());
    Context->PSGetShaderResources(0, 1, SavedSRV0.GetAddressOf());
    Context->PSGetShaderResources(1, 1, SavedSRV1.GetAddressOf());


    ID3D11RenderTargetView* RTV = SavedRTV.Get();
    struct FViewConstants { FMatrix InverseViewProjection; FVector CameraPosition; float Padding; };
    static_assert(sizeof(FViewConstants) == 80);
    const FViewConstants ViewConstants{ ViewProjection.Inverse(), CameraPosition, 0.0f };
    Context->UpdateSubresource(Resources.ViewBuffer.Get(), 0, nullptr, &ViewConstants, 0, 0);
    Context->UpdateSubresource(Resources.FogBuffer.Get(), 0, nullptr, &FogConstants, 0, 0);
    ID3D11Buffer* Buffers[] = { Resources.ViewBuffer.Get(), Resources.FogBuffer.Get() };
    ID3D11ShaderResourceView* InputSRVs[2] = { SceneColor->GetSRV(), SceneDepth->GetSRV() };
    Context->PSSetConstantBuffers(0, 2, Buffers);
    Context->PSSetShaderResources(0, 2, InputSRVs);
    Context->VSSetShader(Resources.VS.Get(), nullptr, 0);
    Context->PSSetShader(Resources.PS.Get(), nullptr, 0);
    Context->GSSetShader(nullptr, nullptr, 0);
    Context->IASetInputLayout(nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->RSSetState(RenderDevice->GetRasterizerState(ERasterizerState::SolidNone));
    Context->OMSetDepthStencilState(RenderDevice->GetDepthStencilState(EDepthStencilState::Disabled), 0);
    Context->OMSetBlendState(RenderDevice->GetBlendState(EBlendState::Opaque), nullptr, 0xffffffff);
    Context->Draw(3, 0);
    if (!Resources.ReportedDraw)
    {
        HTR_LOG(Info, "[Fog] OnRender drew the fog pass. Density={}, Height={}, Falloff={}, MaxOpacity={}",
            FogConstants.FogDensity, FogConstants.FogHeight, FogConstants.FogHeightFalloff, FogConstants.FogMaxOpacity);
        Resources.ReportedDraw = true;
    }

    ID3D11ShaderResourceView* PreviousSRVs[2] = { SavedSRV0.Get(), SavedSRV1.Get() };
    Context->PSSetShaderResources(0, 2, PreviousSRVs);
    ID3D11Buffer* PreviousBuffers[] = { SavedCB0.Get(), SavedCB1.Get() };
    Context->PSSetConstantBuffers(0, 2, PreviousBuffers);
    Context->VSSetShader(SavedVS.Get(), nullptr, 0);
    Context->PSSetShader(SavedPS.Get(), nullptr, 0);
    Context->GSSetShader(SavedGS.Get(), nullptr, 0);
    Context->IASetInputLayout(SavedLayout.Get());
    Context->IASetPrimitiveTopology(SavedTopology);
    Context->RSSetState(SavedRaster.Get());
    Context->OMSetBlendState(SavedBlend.Get(), SavedFactor, SavedMask);
    Context->OMSetDepthStencilState(SavedDepth.Get(), SavedStencil);
    Context->OMSetRenderTargets(1, &RTV, SavedDSV.Get());
    return true;
}

