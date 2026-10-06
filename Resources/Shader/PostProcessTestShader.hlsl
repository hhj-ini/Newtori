
// Todo: Post process

// 후처리 패스에서 읽을 장면 색상.
Texture2D SceneColorTexture : register(t0);
Texture2D<float> SceneDepthTexture : register(t1);

SamplerState SceneColorSampler : register(s0);

cbuffer PostProcessConstants : register(b0)
{
    uint DisplayMode;
    float NearClip;
    float FarClip;
    uint IsOrthographic;
};

struct PSInput
{
	float4 NDCPosition : SV_POSITION;
	float2 UV : TEXCOORD0;
};

PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;

    // 화면 전체를 덮는 큰 삼각형의 세 정점
    if (VertexID == 0)
    {
        Output.UV = float2(0.0f, 0.0f);
    }
    else if (VertexID == 1)
    {
        Output.UV = float2(2.0f, 0.0f);
    }
    else
    {
        Output.UV = float2(0.0f, 2.0f);
    }

    // UV를 클립 좌표로 변환: (0, 0) → (-1, 1)
    float2 ClipXY = float2(Output.UV.x * 2.0f - 1.0f, 1.0f - Output.UV.y * 2.0f);
    Output.NDCPosition = float4(ClipXY, 0.0f, 1.0f);
    
    return Output;
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    if (DisplayMode == 0)
    {
        return SceneColorTexture.Sample(SceneColorSampler, Input.UV);
    }

    int2 Pixel = int2(Input.NDCPosition.xy);
    float NormalizedDepth = SceneDepthTexture.Load(int3(Pixel, 0));

    // 1. 물체가 그려지지 않은 배경은 검정으로 표시한다.
    if (NormalizedDepth >= 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // 2. 깊이값을 렌더용 카메라 기준 거리로 복원한다.
    float ViewDepth;

    if (IsOrthographic != 0)
    {
        ViewDepth = NearClip + NormalizedDepth * (FarClip - NearClip);
    }
    else
    {
        float Denominator = FarClip * (1.0f - NormalizedDepth) + NearClip * NormalizedDepth;
        ViewDepth = NearClip * FarClip / Denominator;
    }

    // 3. 회색으로 표시할 거리 범위를 정한다.
    // 프로젝트의 월드 단위 기준이며, 장면 크기에 맞춰 조정한다.
    const float DepthDisplayRange = 50.0f;
    float DisplayNear;
    float DisplayFar;

    if (IsOrthographic != 0)
    {
        // 현재 GetRenderCamera()의 직교 카메라 후퇴 거리:
        // NearClip + Radius == (NearClip + FarClip) / 2
        float RenderRetreat = (NearClip + FarClip) * 0.5f;

        // 논리 카메라 평면의 앞뒤를 표시 범위로 잡는다.
        float HalfRange = min(DepthDisplayRange, (FarClip - NearClip) * 0.5f);
        
        DisplayNear = RenderRetreat - HalfRange;
        DisplayFar = RenderRetreat + HalfRange;
    }
    else
    {
        DisplayNear = NearClip;
        DisplayFar = min(FarClip, max(DepthDisplayRange, NearClip + 0.001f));
    }

    // 4. 가까운 물체는 어둡게, 먼 물체는 밝게 표시한다.
    float Gray = saturate((ViewDepth - DisplayNear) / max(DisplayFar - DisplayNear, 0.001f));

    return float4(Gray, Gray, Gray, 1.0f);
}
