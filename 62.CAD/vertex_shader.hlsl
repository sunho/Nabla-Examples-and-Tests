#include "common.hlsl"

PSInput VSMain(uint vertexID : SV_VertexID)
{
    const uint vertexIdx = vertexID & 0x3u;
    const uint objectID = vertexID >> 2;
    
    DrawObject drawObj = drawObjects[objectID];
    ObjectType objType = drawObj.type;
    
    PSInput outV;
    outV.color = globals.color;
    outV.lineWidth_eccentricity_objType.x = globals.lineWidth;
    outV.lineWidth_eccentricity_objType.z = (uint)objType;

    if (objType == ObjectType::ELLIPSE)
    {
    }
    else
    {
        double3x3 transformation = (double3x3)globals.viewProjection;
        LinePoints points = vk::RawBufferLoad<LinePoints>(drawObj.address);

        float2 transformedPoints[4u];
        for(uint i = 0u; i < 4u; ++i)
        {
            double2 ndc = mul(transformation, double3(points.p[i], 1)).xy; // Transform to NDC
            transformedPoints[i] = (float2)((ndc + 1.0) * 0.5 * globals.resolution); // Transform to Screen Space
        }

        const float2 lineVector = normalize(transformedPoints[2u] - transformedPoints[1u]);
        const float2 normalToLine = float2(-lineVector.y, lineVector.x);

        if (vertexIdx == 0u || vertexIdx == 1u)
        {
            const float2 vectorPrev = normalize(transformedPoints[1u] - transformedPoints[0u]);
            const float2 normalPrevLine = float2(-vectorPrev.y, vectorPrev.x);
            const float2 miter = normalize(normalPrevLine + normalToLine);

            outV.position.xy = transformedPoints[1u] + (miter * ((float)vertexIdx - 0.5f) * globals.lineWidth) / dot(normalToLine, normalPrevLine);
        }
        else // if (vertexIdx == 2u || vertexIdx == 3u)
        {
            const float2 vectorNext = normalize(transformedPoints[3u] - transformedPoints[2u]);
            const float2 normalNextLine = float2(-vectorNext.y, vectorNext.x);
            const float2 miter = normalize(normalNextLine + normalToLine);

            outV.position.xy = transformedPoints[2u] + (miter * ((float)vertexIdx - 2.5f) * globals.lineWidth) / dot(normalToLine, normalNextLine);
        }

        outV.start_end.xy = transformedPoints[1u];
        outV.start_end.wz = transformedPoints[2u];
        outV.position.xy = outV.position.xy / globals.resolution * 2.0 - 1.0; // back to NDC for SV_Position
        outV.position.w = 1u;
    }
	return outV;
}