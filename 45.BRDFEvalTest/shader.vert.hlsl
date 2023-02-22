struct VSInput
{
	[[vk::location(0)]] float3 Pos;
	[[vk::location(3)]] float3 Normal;
};


float4 gl_Position : SV_POSITION;

#include "common.hlsl"
[[vk::push_constant]] [[vk::offset(0)]] SPushConstants::VertStage pc;

float3 to_right_hand(in float3 v)
{
    return float3(-v.x,v.yz);
}

VSOutput main(VSInput input)
{
	VSOuptut output;
	output.Pos = to_right_hand(input.Pos) + float3(float(gl_InstanceIndex)*1.5,0.f,-1.f);
	gl_Position = pc.ViewProj*float4(output.Pos,1.f);
/*
    outNormal = to_right_hand(normalize(Normal));
    outAlpha = float(gl_InstanceIndex+1)*0.1;

	output.UV = input.UV;
	output.Color = input.Color;
	return output;
*/
}