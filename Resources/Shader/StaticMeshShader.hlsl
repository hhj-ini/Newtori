#pragma pack_matrix(row_major)
cbuffer Viewconstants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
    // 비균등 스케일이 적용된 월드 행렬의 역행렬을 전치하여 노멀을 변환할 때 사용한다.
    matrix InverseTransposeWorld;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float bOpaque;
    float bUnlit;
};


// float3 위치 + float 반경 = 16바이트
// float3 색상 + float 밝기 = 16바이트
cbuffer PointLightConstants : register(b3)
{
    float3 PointLightPosition; // 광원의 월드 위치
    float PointLightAttenuationRadius; // 영향을 주는 최대 거리

    float4 PointLightColor; // 빛의 RGB 색
    float PointLightIntensity; // 빛의 밝기
    float Padding2[3]; // 16바이트 정렬을 위한 패딩
};


struct VS_INPUT
{
    float3 p : POSITION; // Input position from vertex buffer
    float3 n : NORMAL;
    float4 c : COLOR; // Input color from vertex buffer
    float2 t : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    
    //Point Light와 표면 사이의 거리,방향 계산에 필요.
    float3 worldPosition : TEXCOORD1;
};

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

// 임시 하드코딩 Directional Light. 빛이 "향하는" 방향이다.
static const float3 LightDir = normalize(float3(0.2f, 0.2f, -1.0f));
static const float3 LightColor = float3(0.2f, 0.2f, 0.2f);
static const float3 AmbientColor = float3(0.2f, 0.2f, 0.2f);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    output.position = mul(mul(float4(input.p, 1.0f),World), VP);
    // w=0으로 이동 성분을 빼고 월드 공간으로 보낸다. 비균등 스케일이면 역전치가 필요하다.
    output.worldPosition = mul(float4(input.p, 1.0f), World).xyz;
    output.normal = mul(input.n, (float3x3)InverseTransposeWorld);
    
    output.color = input.c;
    output.uv = input.t;
    return output;
}

// 현재 픽셀에 대한 Point Light의 기여를 계산한다.
// N은 정규화된 월드 노멀이다.
float3 CalculatePointLight(float3 worldPosition, float3 N)
{
    float radius = PointLightAttenuationRadius;
    float intensity = clamp(PointLightIntensity, 0.0f, PointLightAttenuationRadius);

    // 라이트가 없거나 비활성 상태인 경우.
    if (radius <= 0.0f || intensity <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // 표면에서 광원을 향하는 벡터.
    float3 toLight = PointLightPosition - worldPosition;

    // 길이를 제곱한 값으로 먼저 영향 범위를 검사한다.
    float distanceSq = dot(toLight, toLight);
    float radiusSq = radius * radius;

    // 영향 반경 밖에서는 이 라이트의 기여가 없다.
    if (distanceSq >= radiusSq)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // 광원 방향을 정규화한다.
    // 표면과 광원의 위치가 같아도 0으로 나누지 않도록 한다.
    float3 L = toLight * rsqrt(max(distanceSq, 1e-6));

    // 표면이 광원을 향할수록 밝고,
    // 반대쪽을 향하면 직접 조명 기여는 0이다.
    // 두 방향 사이 코사인
    float NdotL = saturate(dot(N, L));

    // 영향 반경 경계에서 빛이 갑자기 끊기지 않도록 페이드한다.
    float normalizedDistanceSq = distanceSq / radiusSq;

    float radiusFade = saturate(1.0f - normalizedDistanceSq * normalizedDistanceSq);

    radiusFade *= radiusFade;

    // 기본적으로 거리의 제곱에 반비례하는 감쇠.
    // 광원 바로 근처에서 값이 무한대로 커지지 않도록 제한한다.
     float attenuation = radiusFade / max(distanceSq, 0.01f);

    // 색 * 세기 * 거리 감쇠 * dot(N,L)
    return PointLightColor * intensity * attenuation * NdotL;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    //return g_txColor.Sample(g_Sample, input.uv + UVOffset); // 라이팅 적용 시 제거
    float4 texColor = g_txColor.Sample(g_Sample, input.uv + UVOffset);
    float4 albedo = texColor * BaseColor;
    // Opaque는 알파를 1로 고정한다. 뷰포트 RT를 ImGui가 알파 블렌딩으로 그리므로 알파가 남으면 비쳐 보인다
    float alpha = bOpaque > 0.5f ? 1.0f : albedo.a;
    if(bUnlit)
        return float4(albedo.rgb, alpha);
    
    
    // 보간되면 길이가 틀어지므로 다시 정규화한다
    // 내적해서 각도에 따른 밝기 비율을 구하려고 여기서 길이를 1로 만든다. 
    float3 N = normalize(input.normal);

    float NdotL = saturate(dot(N, -LightDir));
    float3 lighting = AmbientColor + LightColor * NdotL;
    //float3 lighting = float3(1.0f, 1.0f, 1.0f);
    lighting += CalculatePointLight(input.worldPosition, N);
    
    return float4(albedo.rgb * lighting, alpha);
}
