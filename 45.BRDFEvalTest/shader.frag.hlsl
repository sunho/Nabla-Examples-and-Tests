#include "common.hlsl"
[[vk::push_constant]] [[vk::offset(64)]] SPushConstants::FragStage pc;

float4 main(FSInput input) : SV_TARGET
{
    const float3 lightPos = float3(6.75, 4.0, -1.0);
    const float Intensity = 20.0;

    const float3 L = lightPos - Pos;
    const float3 Lnorm = normalize(L);
//    const float3 N = normalize();
    if (false)
    {
        return float4(0.f,1.f,0.f,1.f);
    }
    else
        return float4(1.f,0.f,0.f,1.f);
}