enum BRDFTestNumber : uint32_t
{
    TEST_GGX = 1,
    TEST_BECKMANN,
    TEST_PHONG,
    TEST_AS,
    TEST_OREN_NAYAR,
    TEST_LAMBERT,
};

struct SPushConstants
{
    struct VertStage
    {
        float4x4 ViewProj;
    } vertStage;
    struct FragStage
    {
        float3 campos;
        BRDFTestNumber testNum;
    } fragStage;
};

// for the shaders
#ifndef __cplusplus
struct VSOuptut
{
//	[[vk::location(0)]] float3 Normal;
	[[vk::location(1)]] float3 Pos;
//	[[vk::location(2)]] nointerpolation float Alpha;
};
using FSInput = VSOutput;
#endif