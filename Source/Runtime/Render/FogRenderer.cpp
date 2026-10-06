#include "EnginePCH.h"
#include "FogRenderer.h"
#include "RenderDevice.h"
#include "RenderUtil.h"

bool FFogRenderer::Init(FRenderDevice* InRenderDevice)
{
    RenderDevice = InRenderDevice;
    return RenderDevice != nullptr;
}

void FFogRenderer::OnRender(const FMatrix& ViewProjection, const FVector& CameraPosition, const FHeightFogConstants& FogConstants)
{
    if (!RenderDevice || FogConstants.FogDensity <= 0.0f) return;
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
            return;
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
        D3D11_BLEND_DESC BlendDesc{};
        auto& Target = BlendDesc.RenderTarget[0];
        Target.BlendEnable = TRUE;
        Target.SrcBlend = D3D11_BLEND_ONE;
        Target.DestBlend = D3D11_BLEND_SRC_ALPHA;
        Target.BlendOp = D3D11_BLEND_OP_ADD;

        Target.SrcBlendAlpha = D3D11_BLEND_ZERO;
        Target.DestBlendAlpha = D3D11_BLEND_ONE;
        Target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        Target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (SUCCEEDED(Hr)) Hr = Device->CreateBlendState(&BlendDesc, Resources.Blend.GetAddressOf());
        if (FAILED(Hr))
        {
            Resources.Blend.Reset();
            HTR_LOG(Error, "[FogTest] resource creation failed: {}", static_cast<uint32>(Hr));
        }
    }
    if (!Resources.Blend) return;

    ComPtr<ID3D11DepthStencilView> InputDSV;
    Context->OMGetRenderTargets(0, nullptr, InputDSV.GetAddressOf());
    if (!InputDSV)
    {
        if (!Resources.ReportedMissingDepth)
        {
            HTR_LOG(Warning, "[Fog] OnRender requires the scene depth DSV to be bound before the pass.");
            Resources.ReportedMissingDepth = true;
        }
        return;
    }
    ComPtr<ID3D11Resource> DepthResource;
    InputDSV->GetResource(DepthResource.GetAddressOf());
    ComPtr<ID3D11Texture2D> SourceDepth;
    if (FAILED(DepthResource->QueryInterface(IID_PPV_ARGS(SourceDepth.GetAddressOf())))) return;
    D3D11_TEXTURE2D_DESC DepthDesc{};
    SourceDepth->GetDesc(&DepthDesc);
    if (DepthDesc.Format != DXGI_FORMAT_D24_UNORM_S8_UINT || DepthDesc.SampleDesc.Count != 1)
        return; // This temporary path only supports the current viewport depth format.
    if (!Resources.DepthSRV || Resources.Width != DepthDesc.Width || Resources.Height != DepthDesc.Height)
    {
        Resources.DepthSRV.Reset();
        Resources.DepthCopy.Reset();
        DepthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        DepthDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        DepthDesc.Usage = D3D11_USAGE_DEFAULT;
        DepthDesc.CPUAccessFlags = 0;
        DepthDesc.MiscFlags = 0;
        HRESULT Hr = Device->CreateTexture2D(&DepthDesc, nullptr, Resources.DepthCopy.GetAddressOf());
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
        SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = 1;
        if (SUCCEEDED(Hr)) Hr = Device->CreateShaderResourceView(Resources.DepthCopy.Get(), &SRVDesc, Resources.DepthSRV.GetAddressOf());
        if (FAILED(Hr))
        {
            HTR_LOG(Error, "[FogTest] depth copy creation failed: {}", static_cast<uint32>(Hr));
            return;
        }
        Resources.Width = DepthDesc.Width;
        Resources.Height = DepthDesc.Height;
    }

    // Save all state changed below; the following Grid/Translucency passes keep their behavior.
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
    ComPtr<ID3D11ShaderResourceView> SavedSRV;
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
    Context->PSGetShaderResources(0, 1, SavedSRV.GetAddressOf());

    Context->OMSetRenderTargets(0, nullptr, nullptr);
    Context->CopyResource(Resources.DepthCopy.Get(), SourceDepth.Get());
    ID3D11RenderTargetView* RTV = SavedRTV.Get();
    Context->OMSetRenderTargets(1, &RTV, nullptr); // Preserve scene color; no clear, no writable depth.
    struct FViewConstants { FMatrix InverseViewProjection; FVector CameraPosition; float Padding; };
    static_assert(sizeof(FViewConstants) == 80);
    const FViewConstants ViewConstants{ ViewProjection.Inverse(), CameraPosition, 0.0f };
    Context->UpdateSubresource(Resources.ViewBuffer.Get(), 0, nullptr, &ViewConstants, 0, 0);
    Context->UpdateSubresource(Resources.FogBuffer.Get(), 0, nullptr, &FogConstants, 0, 0);
    ID3D11Buffer* Buffers[] = { Resources.ViewBuffer.Get(), Resources.FogBuffer.Get() };
    ID3D11ShaderResourceView* DepthSRV = Resources.DepthSRV.Get();
    Context->PSSetConstantBuffers(0, 2, Buffers);
    Context->PSSetShaderResources(0, 1, &DepthSRV);
    Context->VSSetShader(Resources.VS.Get(), nullptr, 0);
    Context->PSSetShader(Resources.PS.Get(), nullptr, 0);
    Context->GSSetShader(nullptr, nullptr, 0);
    Context->IASetInputLayout(nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->RSSetState(RenderDevice->GetRasterizerState(ERasterizerState::SolidNone));
    Context->OMSetDepthStencilState(RenderDevice->GetDepthStencilState(EDepthStencilState::Disabled), 0);
    Context->OMSetBlendState(Resources.Blend.Get(), nullptr, 0xffffffff);
    Context->Draw(3, 0);
    if (!Resources.ReportedDraw)
    {
        HTR_LOG(Info, "[Fog] OnRender drew the fog pass. Density={}, Height={}, Falloff={}, MaxOpacity={}",
            FogConstants.FogDensity, FogConstants.FogHeight, FogConstants.FogHeightFalloff, FogConstants.FogMaxOpacity);
        Resources.ReportedDraw = true;
    }

    ID3D11ShaderResourceView* PreviousSRV = SavedSRV.Get();
    Context->PSSetShaderResources(0, 1, &PreviousSRV);
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
}

