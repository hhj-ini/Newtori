#pragma pack_matrix(row_major)

cbuffer Constants : register(b0)
{
    matrix InverseViewProjection;
    float3 CameraPosition;
    float  Padding;
};

cbuffer HeightFogConstants : register(b1)
{
    float4 FogInscatteringColor;
    float FogHeight;
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;
    float2 padding;
};

Texture2D    SceneDepthTexture : register(t0);


struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;

    // VertexID 0,1,2 -> UV (0,0) (2,0) (0,2)
    // 화면보다 큰 삼각형 하나로 전체를 덮는다. 사각형 두 장보다 대각선 이음매가 없다.
    Output.UV = float2((VertexID << 1) & 2, VertexID & 2);

    // UV [0,2] -> NDC [-1,3]. y는 위아래가 뒤집혀 있다.
    Output.Position = float4(Output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f);

    return Output;
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    // 화면 좌표 -> 원평면 위의 월드 좌표 -> 카메라가 그쪽을 보는 방향
    float2 NDC = Input.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    
    float Depth = SceneDepthTexture.Load(int3(int2(Input.Position.xy), 0)).r;
    int2 Pixel = int2(Input.Position.xy);

    // 픽셀의 월드 좌표 계산
    float4 WorldPoint = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    // 원근 나눗셈. 빼먹으면 화면 가장자리가 틀어진다
    WorldPoint /= WorldPoint.w;                      
    
    
    // Exponential Height Fog 계산
    
    float3 Ray = WorldPoint.xyz - CameraPosition;
    
    float RayLength = length(Ray); // 카메라에서 월드 포인트까지의 거리
    float Start = clamp(StartDistance, 0.0f, RayLength); // 안개 시작 지점
    float FogLength = RayLength - Start; // 안개가 적용되는 거리
    float3 Direction = Ray / RayLength; // 카메라에서 월드 포인트까지의 방향 벡터
    float StartHeight = CameraPosition.z + Start * Direction.z; // 안개 시작 지점의 높이
    
    // 시작지점 전까지는 색깔을 원색으로 맞춤
    if (FogLength <= 0.0f)
    {
        return float4(0, 0, 0, 1);
    }
    
    
    // 안개 시작 지점의 밀도 계산. exp2는 2의 거듭제곱을 계산하는 함수
    float StartDensity = FogDensity * exp2(-FogHeightFalloff * (StartHeight - FogHeight)); // 안개 시작 지점의 밀도
    
    // 위치별 안개 밀도를 다 더 해서 누적 안개량을 계산. (선적분)
    float HeightDelta = WorldPoint.z - StartHeight; // 안개 시작 지점과 월드 포인트의 높이 차이
    float q = FogHeightFalloff * HeightDelta; // 높이 차이에 따른 안개 감소율
    float IntergralFactor;
    if (abs(q) < 0.0001f)
    {
        IntergralFactor = 1 - (q * log(2.0))/ 2.0; // q가 0에 가까우면 테일러 전개로 근사
    }
    else
    {    
        IntergralFactor = (1 - exp2(-q)) / (q * log(2.0)); // q가 0이 아니면 일반적인 지수 함수 계산
    }
    
    // 안개 총량
    float OpticalDepth = StartDensity * FogLength * IntergralFactor;
    
    // Beer-Lambert 법칙을 이용한 안개 투과율 계산
    float T = exp2(-OpticalDepth); // 안개 투과율
    if ((1.0 - T) > FogMaxOpacity)
    {
        T = 1.0 - FogMaxOpacity; // 안개 최대 불투명도 제한
    }
    
    
    // InScattering Color 계산
    float3 InScattering = FogInscatteringColor.rgb * (1 - T); // 안개에 의해 산란된 빛의 색상
    
    // 블렌딩 설정
    // SrcBlend  = D3D11_BLEND_ONE;
    // DestBlend = D3D11_BLEND_SRC_ALPHA;
    return float4(InScattering, T);
    
    

}
