#pragma shader_stage(vertex)

#include "common.hlsl"

float2 intersectLines2D(in float2 p1, in float2 v1, in float2 p2, in float2 v2)
{
    float det = v1.y * v2.x - v1.x * v2.y;
    float2x2 inv = float2x2(v2.y, -v2.x, v1.y, -v1.x) / det;
    float2 t = mul(inv, p1 - p2);
    return p2 + mul(v2, t.y);
}

PSInput main(uint vertexID : SV_VertexID)
{
    const uint vertexIdx = vertexID & 0x3u;
    const uint objectID = vertexID >> 2;

    DrawObject drawObj = drawObjects[objectID];
    LineStyle lineStyle = lineStyles[drawObj.styleIdx];
    ObjectType objType = drawObj.type;

    PSInput outV;

    const float screenSpaceLineWidth = lineStyle.screenSpaceLineWidth + float(lineStyle.worldSpaceLineWidth * globals.screenToWorldRatio);
    const float antiAliasedLineWidth = screenSpaceLineWidth + globals.antiAliasingFactor * 2.0f;
    outV.color = lineStyle.color;

    outV.lineWidth_eccentricity_objType_writeToAlpha.x = asuint(screenSpaceLineWidth);
    outV.lineWidth_eccentricity_objType_writeToAlpha.z = (uint)objType;
    outV.lineWidth_eccentricity_objType_writeToAlpha.w = (vertexIdx % 2u == 0u) ? 1u : 0u;

    if (objType == ObjectType::ELLIPSE)
    {
#ifdef LOAD_STRUCT
        PackedEllipseInfo ellipse = vk::RawBufferLoad<PackedEllipseInfo>(drawObj.address, 8u);
        double2 majorAxis = ellipse.majorAxis;
        double2 center = ellipse.majorAxis;
#else
        double2 majorAxis = vk::RawBufferLoad<double2>(drawObj.address, 8u);
        double2 center = vk::RawBufferLoad<double2>(drawObj.address + 16u, 8u);
#endif 
        uint4 angleBoundsPacked_eccentricityPacked_pad = vk::RawBufferLoad<uint4>(drawObj.address + 32u, 8u);

        outV.lineWidth_eccentricity_objType_writeToAlpha.y = angleBoundsPacked_eccentricityPacked_pad.z; // asfloat because it is acrually packed into a uint and we should not treat it as a float yet.

        double3x3 transformation = (double3x3)globals.viewProjection;

        double2 ndcCenter = mul(transformation, double3(center, 1)).xy; // Transform to NDC
        float2 transformedCenter = (float2)((ndcCenter + 1.0) * 0.5 * globals.resolution); // Transform to Screen Space
        outV.start_end.xy = transformedCenter;

        double2 ndcMajorAxis = mul(transformation, double3(majorAxis, 0)).xy; // Transform to NDC
        float2 transformedMajorAxis = (float2)((ndcMajorAxis) * 0.5 * globals.resolution); // Transform to Screen Space
        outV.start_end.zw = transformedMajorAxis;

        // construct a cage, working in ellipse screen space (ellipse centered in 0,0 and major axis aligned with x-axis) :
        const float2 angleBounds = ((float2)(angleBoundsPacked_eccentricityPacked_pad.xy) / UINT32_MAX) * (2.0 * nbl_hlsl_PI);
        const float eccentricity = (float)(angleBoundsPacked_eccentricityPacked_pad.z) / UINT32_MAX;
        outV.ellipseBounds_bezierP2P3.xy = angleBounds;

        // TODO: Optimize
        float majorAxisLength = length(transformedMajorAxis);
        float minorAxisLength = float(majorAxisLength * eccentricity);
        float2 ab = float2(majorAxisLength, minorAxisLength);
        float2 start = float2(ab * float2(cos(angleBounds.x), sin(angleBounds.x)));
        float2 end = float2(ab * float2(cos(angleBounds.y), sin(angleBounds.y)));
        float2 startToEnd = end - start;

        float2 tangentToStartPoint = normalize(float2(-majorAxisLength * sin(angleBounds.x), minorAxisLength * cos(angleBounds.x)));
        float2 tangentToEndPoint = normalize(float2(-majorAxisLength * sin(angleBounds.y), minorAxisLength * cos(angleBounds.y)));
        float2 normalToStartPoint = float2(tangentToStartPoint.y, -tangentToStartPoint.x);
        float2 normalToEndPoint = float2(tangentToEndPoint.y, -tangentToEndPoint.x);

        bool roundedJoins = true;
        if (roundedJoins)
        {
            start -= tangentToStartPoint * antiAliasedLineWidth * 0.5f;
            end += tangentToEndPoint * antiAliasedLineWidth * 0.5f;
        }

        if (vertexIdx == 0u)
        {
            outV.position.xy = start - (normalToStartPoint)*antiAliasedLineWidth * 0.5f;
        }
        else if (vertexIdx == 1u)
        {
            // find the theta (input to ellipse) in which the tangent of the ellipse is equal to startToEnd:
            float theta = atan2(eccentricity * startToEnd.x, -startToEnd.y) + nbl_hlsl_PI;
            float2 perp = normalize(float2(startToEnd.y, -startToEnd.x));
            float2 p = float2(ab * float2(cos(theta), sin(theta)));
            float2 intersection = intersectLines2D(p + perp * antiAliasedLineWidth * 0.5f, startToEnd, start, normalToStartPoint);
            outV.position.xy = intersection;
        }
        else if (vertexIdx == 2u)
        {
            outV.position.xy = end - (normalToEndPoint)*antiAliasedLineWidth * 0.5f;
        }
        else
        {
            // find the theta (input to ellipse) in which the tangent of the ellipse is equal to startToEnd:
            float theta = atan2(eccentricity * startToEnd.x, -startToEnd.y) + nbl_hlsl_PI;
            float2 perp = normalize(float2(startToEnd.y, -startToEnd.x));
            float2 p = float2(ab * float2(cos(theta), sin(theta)));
            float2 intersection = intersectLines2D(p + perp * antiAliasedLineWidth * 0.5f, startToEnd, end, normalToEndPoint);
            outV.position.xy = intersection;
        }

        // Transform from ellipse screen space to actual screen space with a rotation and translation
        float2 dir = normalize(transformedMajorAxis);
        outV.position.xy = mul(float2x2(dir.x, dir.y, dir.y, -dir.x), outV.position.xy); // transforming ellipse -> screen space we need to also invert y other than rotate, hence the inversion of signs in the last row
        outV.position.xy += transformedCenter;

        // Transform to ndc
        outV.position.xy = (outV.position.xy / globals.resolution) * 2.0 - 1.0; // back to NDC for SV_Position
        outV.position.w = 1u;
    }
    else if (objType == ObjectType::LINE)
    {
        double3x3 transformation = (double3x3)globals.viewProjection;

        double2 points[2u];
        points[0u] = vk::RawBufferLoad<double2>(drawObj.address, 8u);
        points[1u] = vk::RawBufferLoad<double2>(drawObj.address + sizeof(double2), 8u);

        float2 transformedPoints[2u];
        for (uint i = 0u; i < 2u; ++i)
        {
            double2 ndc = mul(transformation, double3(points[i], 1)).xy; // Transform to NDC
            transformedPoints[i] = (float2)((ndc + 1.0) * 0.5 * globals.resolution); // Transform to Screen Space
        }

        const float2 lineVector = normalize(transformedPoints[1u] - transformedPoints[0u]);
        const float2 normalToLine = float2(-lineVector.y, lineVector.x);

        if (vertexIdx == 0u || vertexIdx == 1u)
        {
            // work in screen space coordinates because of fixed pixel size
            outV.position.xy = transformedPoints[0u]
                + normalToLine * (((float)vertexIdx - 0.5f) * antiAliasedLineWidth)
                - lineVector * antiAliasedLineWidth * 0.5f;
        }
        else // if (vertexIdx == 2u || vertexIdx == 3u)
        {
            // work in screen space coordinates because of fixed pixel size
            outV.position.xy = transformedPoints[1u]
                + normalToLine * (((float)vertexIdx - 2.5f) * antiAliasedLineWidth)
                + lineVector * antiAliasedLineWidth * 0.5f;
        }

        outV.start_end.xy = transformedPoints[0u];
        outV.start_end.zw = transformedPoints[1u];

        // convert back to ndc
        outV.position.xy = (outV.position.xy / globals.resolution) * 2.0 - 1.0; // back to NDC for SV_Position
        outV.position.w = 1u;
    }
    else if (objType == ObjectType::QUAD_BEZIER)
    {
        double3x3 transformation = (double3x3)globals.viewProjection;

        double2 points[3u];
        points[0u] = vk::RawBufferLoad<double2>(drawObj.address, 8u);
        points[1u] = vk::RawBufferLoad<double2>(drawObj.address + sizeof(double2), 8u);
        points[2u] = vk::RawBufferLoad<double2>(drawObj.address + sizeof(double2) * 2u, 8u);

        // transform these points into screen space and pass to fragment
        float2 transformedPoints[3u];
        for (uint i = 0u; i < 3u; ++i)
        {
            double2 ndc = mul(transformation, double3(points[i], 1)).xy; // Transform to NDC
            transformedPoints[i] = (float2)((ndc + 1.0) * 0.5 * globals.resolution); // Transform to Screen Space
        }

        outV.start_end.xy = transformedPoints[0u];
        outV.start_end.zw = transformedPoints[1u];
        outV.ellipseBounds_bezierP2P3.xy = transformedPoints[2u];

        if (vertexIdx == 0u)
            outV.position = float4(-1, -1, 0, 1);
        else if (vertexIdx == 1u)
            outV.position = float4(-1, +1, 0, 1);
        else if (vertexIdx == 2u)
            outV.position = float4(+1, -1, 0, 1);
        else if (vertexIdx == 3u)
            outV.position = float4(+1, +1, 0, 1);
    }
    else if (objType == ObjectType::CUBIC_BEZIER)
    {
        double3x3 transformation = (double3x3)globals.viewProjection;

        double2 points[4u];
        points[0u] = vk::RawBufferLoad<double2>(drawObj.address, 8u);
        points[1u] = vk::RawBufferLoad<double2>(drawObj.address + sizeof(double2), 8u);
        points[2u] = vk::RawBufferLoad<double2>(drawObj.address + sizeof(double2) * 2u, 8u);
        points[3u] = vk::RawBufferLoad<double2>(drawObj.address + sizeof(double2) * 3u, 8u);

        // transform these points into screen space and pass to fragment
        float2 transformedPoints[4u];
        for (uint i = 0u; i < 4u; ++i)
        {
            double2 ndc = mul(transformation, double3(points[i], 1)).xy; // Transform to NDC
            transformedPoints[i] = (float2)((ndc + 1.0) * 0.5 * globals.resolution); // Transform to Screen Space
        }

        outV.start_end.xy = transformedPoints[0u];
        outV.start_end.zw = transformedPoints[1u];
        outV.ellipseBounds_bezierP2P3.xy = transformedPoints[2u];
        outV.ellipseBounds_bezierP2P3.zw = transformedPoints[3u];

        if (vertexIdx == 0u)
            outV.position = float4(-1, -1, 0, 1);
        else if (vertexIdx == 1u)
            outV.position = float4(-1, +1, 0, 1);
        else if (vertexIdx == 2u)
            outV.position = float4(+1, -1, 0, 1);
        else if (vertexIdx == 3u)
            outV.position = float4(+1, +1, 0, 1);
    }

#if 0
    if (vertexIdx == 0u)
        outV.position = float4(-1, -1, 0, 1);
    else if (vertexIdx == 1u)
        outV.position = float4(-1, +1, 0, 1);
    else if (vertexIdx == 2u)
        outV.position = float4(+1, -1, 0, 1);
    else if (vertexIdx == 3u)
        outV.position = float4(+1, +1, 0, 1);
#endif

    return outV;
}