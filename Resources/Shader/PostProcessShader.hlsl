
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
    
    // FXAA 설정
    uint EnableFXAA;
    float EdgeThreshold;
    float EdgeThresholdMin;
    float SubpixelStrength;
};

struct PSInput
{
	float4 NDCPosition : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float CalculateLuma(float2 UV)
{
    float3 Color = SceneColorTexture.SampleLevel(SceneColorSampler, UV, 0).rgb;

    return dot(Color, float3(0.299f, 0.587f, 0.114f));
}

float4 ApplyFXAA(float2 UV)
{
    // 경계 끝을 찾을 때 이동하는 추가 거리
    // 가까운 끝은 촘촘히, 먼 끝은 큰 보폭으로 탐색함
    const float SearchSteps[12] =
    {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f,
        2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f
    };

    const uint MAX_SEARCH_STEPS_COUNT = 12;
    
    // 한 픽셀에 해당하는 UV 간격 구하기
    // 4분할 뷰마다 텍스처 크기가 다르므로, 입력 텍스처에서 직접 구함
    uint Width;
    uint Height;
    
    SceneColorTexture.GetDimensions(Width, Height);
    float2 TexelSize = 1.0f / float2(Width, Height);
    
    float4 CenterColor = SceneColorTexture.SampleLevel(SceneColorSampler, UV, 0);
    float LumaCenter = dot(CenterColor.rgb, float3(0.299f, 0.587f, 0.114f));
    
    float LumaNorth = CalculateLuma(UV + float2(0.0f, -TexelSize.y));
    float LumaSouth = CalculateLuma(UV + float2(0.0f, TexelSize.y));
    float LumaWest = CalculateLuma(UV + float2(-TexelSize.x, 0.0f));
    float LumaEast = CalculateLuma(UV + float2(TexelSize.x, 0.0f));

    float LumaMin = min(LumaCenter, min(min(LumaNorth, LumaSouth), min(LumaWest, LumaEast)));
    float LumaMax = max(LumaCenter, max(max(LumaNorth, LumaSouth), max(LumaWest, LumaEast)));
    float LumaRange = LumaMax - LumaMin;

    // 중앙과 상하좌우의 대비가 낮으면 AA가 필요 없다. 따라서, 평탄한 영역에서는 추가 샘플링을 피함
    if (LumaRange < max(EdgeThresholdMin, LumaMax * EdgeThreshold))
    {
        return CenterColor;
    }

    float LumaNW = CalculateLuma(UV + float2(-TexelSize.x, -TexelSize.y));
    float LumaNE = CalculateLuma(UV + float2(TexelSize.x, -TexelSize.y));
    float LumaSW = CalculateLuma(UV + float2(-TexelSize.x, TexelSize.y));
    float LumaSE = CalculateLuma(UV + float2(TexelSize.x, TexelSize.y));

    // 위아래 밝기 변화가 크면 가로로 뻗은 경계
    // 좌우 밝기 변화가 크면 세로로 뻗은 경계
    float EdgeHorizontal =
        2.0f * abs(LumaNorth + LumaSouth - 2.0f * LumaCenter)
        + abs(LumaNW + LumaSW - 2.0f * LumaWest)
        + abs(LumaNE + LumaSE - 2.0f * LumaEast);

    float EdgeVertical =
        2.0f * abs(LumaWest + LumaEast - 2.0f * LumaCenter)
        + abs(LumaNW + LumaNE - 2.0f * LumaNorth)
        + abs(LumaSW + LumaSE - 2.0f * LumaSouth);

    bool bHorizontal = (EdgeHorizontal >= EdgeVertical) ? true : false;

    // 가로 경계는 위/아래, 세로 경계는 왼쪽/오른쪽 중, 
    // 중앙과의 밝기 차이가 더 큰 쪽을 선택
    float LumaNegative = bHorizontal ? LumaNorth : LumaWest;
    float LumaPositive = bHorizontal ? LumaSouth : LumaEast;
    
    float GradientNegative = abs(LumaNegative - LumaCenter);
    float GradientPositive = abs(LumaPositive - LumaCenter);
    
    bool bChooseNegative = (GradientNegative >= GradientPositive) ? true : false;
    float Gradient = max(GradientNegative, GradientPositive);
    if (Gradient < 1.0e-5f)
    {
        return CenterColor;
    }

    float NeighborLuma = bChooseNegative ? LumaNegative : LumaPositive;
    float LocalAverage = 0.5f * (LumaCenter + NeighborLuma);

    // NormalStep은 최종 색을 섞을 방향
    // 부호까지 포함하므로 마지막 UV 이동에서 다시 방향을 판단하지 않음
    float2 NormalStep = bHorizontal ? float2(0.0f, TexelSize.y) : float2(TexelSize.x, 0.0f);
    if (bChooseNegative)
    {
        NormalStep = -NormalStep;
    }

    // TangentStep은 경계를 따라 양쪽 끝을 찾는 방향
    float2 TangentStep = bHorizontal ? float2(TexelSize.x, 0.0f) : float2(0.0f, TexelSize.y);

    // 중앙 픽셀과 선택한 이웃 픽셀 사이의 경계에서 탐색을 시작
    float2 EdgeUV = UV + 0.5f * NormalStep;
    float2 NegativeUV = EdgeUV;
    float2 PositiveUV = EdgeUV;
    
    float NegativeEndLuma = LocalAverage;
    float PositiveEndLuma = LocalAverage;
    
    bool bFoundNegative = false;
    bool bFoundPositive = false;
    float SearchDistance = 0.0f;
    
    [loop]
    for (uint StepIndex = 0; StepIndex < MAX_SEARCH_STEPS_COUNT; ++StepIndex)
    {
        SearchDistance += SearchSteps[StepIndex];

        if (bFoundNegative == false)
        {
            NegativeUV = EdgeUV - TangentStep * SearchDistance;
            NegativeEndLuma = CalculateLuma(NegativeUV);
            
            bFoundNegative = (abs(NegativeEndLuma - LocalAverage) >= Gradient * 0.25f) ? true : false;
        }

        if (bFoundPositive == false)
        {
            PositiveUV = EdgeUV + TangentStep * SearchDistance;
            PositiveEndLuma = CalculateLuma(PositiveUV);
            
            bFoundPositive = (abs(PositiveEndLuma - LocalAverage) >= Gradient * 0.25f) ? true : false;
        }

        if (bFoundNegative && bFoundPositive)
        {
            break;
        }
    }

    float NegativeDistance = bHorizontal ? UV.x - NegativeUV.x : UV.y - NegativeUV.y;
    float PositiveDistance = bHorizontal ? PositiveUV.x - UV.x : PositiveUV.y - UV.y;

    bool bNearestIsNegative = (NegativeDistance < PositiveDistance) ? true : false;
    bool bFoundNearest = bNearestIsNegative ? bFoundNegative : bFoundPositive;
    float NearestEndLuma = bNearestIsNegative ? NegativeEndLuma : PositiveEndLuma;

    // 가까운 끝이 현재 픽셀과 반대 밝기 방향을 가리킬 때만 경계 길이로 계산한 오프셋을 사용
    bool bGoodSpan = (bFoundNearest && ((NearestEndLuma < LocalAverage) != (LumaCenter < LocalAverage))) ? true : false;

    float SpanLength = max(NegativeDistance + PositiveDistance, 1.0e-6f);
    float EdgeOffset = bGoodSpan ? (0.5f - min(NegativeDistance, PositiveDistance) / SpanLength) : 0.0f;

    // 고립된 작은 픽셀이나 끊긴 얇은 선을 위한 서브픽셀 보정
    float SurroundingAverage = (2.0f * (LumaNorth + LumaSouth + LumaWest + LumaEast) + LumaNW + LumaNE + LumaSW + LumaSE) / 12.0f;
    float Subpixel = saturate(abs(SurroundingAverage - LumaCenter) / LumaRange);
    Subpixel = Subpixel * Subpixel * (3.0f - 2.0f * Subpixel);
    
    float SubpixelOffset = Subpixel * Subpixel * SubpixelStrength;

    // 두 보정 중 더 큰 이동량으로 색을 한 번만 읽음
    float FinalOffset = max(EdgeOffset, SubpixelOffset);
    float3 FilteredColor = SceneColorTexture.SampleLevel(SceneColorSampler, UV + NormalStep * FinalOffset, 0).rgb;

    return float4(FilteredColor, CenterColor.a);
}

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
        if (EnableFXAA != 0)
        {
            return ApplyFXAA(Input.UV);
        }

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
