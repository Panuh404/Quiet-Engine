#version 450

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 View;
    mat4 Projection;
} ubo;

layout(push_constant) uniform EditorGridData
{
    vec4 MinorColor;
    vec4 MajorColor;
    vec4 XAxisColor;
    vec4 ZAxisColor;

    // x = base minor spacing
    // y = camera-height LOD reference
    // z = base fade-start distance
    // w = base fade-end distance
    vec4 Parameters;
} pc;

layout(location = 0) in vec3 vNearPoint;
layout(location = 1) in vec3 vFarPoint;
layout(location = 2) flat in vec3 vCameraPosition;

layout(location = 0) out vec4 outColor;

float GridLineMask(const vec2 worldCoordinate, const float spacing)
{
    const vec2 scaledCoordinate = worldCoordinate / max(spacing, 0.0001);
    const vec2 coordinateWidth = max(fwidth(scaledCoordinate), vec2(0.0001));
    const vec2 distanceToLine = abs(fract(scaledCoordinate - 0.5) - 0.5) / coordinateWidth;

    return 1.0 - min(min(distanceToLine.x, distanceToLine.y), 1.0);
}

float AxisLineMask(const float distanceToAxis)
{
    const float axisWidth = max(fwidth(distanceToAxis), 0.0001);
    return 1.0 - clamp(abs(distanceToAxis) / axisWidth, 0.0, 1.0);
}

float CalculateGridLOD(const float cameraHeight, const float referenceHeight)
{
    const float heightRatio = max(cameraHeight / max(referenceHeight, 0.0001), 1.0);

    // GLSL has no dedicated log10 function.
    return log2(heightRatio) / log2(10.0);
}

void main()
{
    const vec3 rayDirection = vFarPoint - vNearPoint;
    if (abs(rayDirection.y) <= 0.000001)
    {
        discard;
    }

    const float intersectionDistance = -vNearPoint.y / rayDirection.y;
    if (intersectionDistance < 0.0 || intersectionDistance > 1.0)
    {
        discard;
    }

    const vec3 worldPosition = vNearPoint + rayDirection * intersectionDistance;
    const float baseSpacing = max(pc.Parameters.x, 0.0001);
    const float lodReference = max(pc.Parameters.y, baseSpacing);
    const float cameraHeight = max(abs(vCameraPosition.y), baseSpacing);

    const float lod = CalculateGridLOD(cameraHeight, lodReference);
    const float lodLevel = floor(lod);

    // Delay the transition slightly at each end of the
    // decade to keep the grid visually stable.
    const float lodBlend = smoothstep(0.15, 0.85, fract(lod));
    const float levelScale = pow(10.0, lodLevel);

    const float fineSpacing = baseSpacing * levelScale;
    const float mediumSpacing = fineSpacing * 10.0;
    const float coarseSpacing = mediumSpacing * 10.0;

    const float fineMask = GridLineMask(worldPosition.xz, fineSpacing);
    const float mediumMask = GridLineMask(worldPosition.xz, mediumSpacing);
    const float coarseMask = GridLineMask(worldPosition.xz, coarseSpacing);

    // The fine level fades away while the medium level changes
    // from a major line into the next decade's minor line.
    // Simultaneously, the next coarse major level fades in.
    const float fineAlpha = fineMask * pc.MinorColor.a * (1.0 - lodBlend);
    const vec3 mediumColor = mix(pc.MajorColor.rgb, pc.MinorColor.rgb, lodBlend);

    const float mediumAlpha = mediumMask * mix(pc.MajorColor.a, pc.MinorColor.a, lodBlend);
    const float coarseAlpha = coarseMask * pc.MajorColor.a * lodBlend;

    const float colorWeight = fineAlpha + mediumAlpha + coarseAlpha;

    vec3 gridColor = pc.MinorColor.rgb;

    if (colorWeight > 0.0001)
    {
        gridColor = (pc.MinorColor.rgb * fineAlpha + mediumColor * mediumAlpha + pc.MajorColor.rgb * coarseAlpha) / colorWeight;
    }

    float gridAlpha = max(fineAlpha, max(mediumAlpha, coarseAlpha));

    // The X axis extends along world X, so its distance
    // from the axis is measured on world Z.
    const float xAxisMask = AxisLineMask(worldPosition.z);
    gridColor = mix(gridColor, pc.XAxisColor.rgb, xAxisMask);
    gridAlpha = max(gridAlpha, xAxisMask * pc.XAxisColor.a);

    // The Z axis extends along world Z, so its distance
    // from the axis is measured on world X.
    const float zAxisMask = AxisLineMask(worldPosition.x);
    gridColor = mix(gridColor, pc.ZAxisColor.rgb, zAxisMask);
    gridAlpha = max(gridAlpha, zAxisMask * pc.ZAxisColor.a);

    const vec3 vectorToCamera = vCameraPosition - worldPosition;
    const float cameraDistance = length(vectorToCamera);
    const float maximumRayDistance = max(length(vFarPoint - vCameraPosition), 0.0001);

    // Scale the visible grid range continuously with the same
    // camera-height decade used by the line-density LOD.
    const float continuousScale = pow(10.0, lod);
    const float requestedFadeEnd = max(pc.Parameters.w * continuousScale, 0.0001);

    const float fadeEnd = min(requestedFadeEnd, maximumRayDistance * 0.95);
    const float requestedFadeStart = max(pc.Parameters.z * continuousScale, 0.0);

    const float fadeStart = min(requestedFadeStart, fadeEnd * 0.65);
    const float distanceFade = 1.0 - smoothstep(fadeStart, max(fadeEnd, fadeStart + 0.0001), cameraDistance);

    // Fade fragments whose view direction becomes almost parallel
    // to the ground plane. This suppresses horizon shimmer.
    const float planeFacing = abs(vectorToCamera.y) / max(cameraDistance, 0.0001);
    const float horizonFade = smoothstep(0.015, 0.080, planeFacing);
    gridAlpha *= distanceFade * horizonFade;
    if (gridAlpha <= 0.01)
    {
        discard;
    }

    const vec4 clipPosition = ubo.Projection * ubo.View * vec4(worldPosition, 1.0);
    if (clipPosition.w <= 0.000001)
    {
        discard;
    }

    const float fragmentDepth = clipPosition.z / clipPosition.w;
    if (fragmentDepth < 0.0 || fragmentDepth > 1.0)
    {
        discard;
    }

    // Keep coplanar mesh surfaces in front of the editor grid.
    gl_FragDepth = min(fragmentDepth + 0.00001, 1.0);
    outColor = vec4(gridColor, gridAlpha);
}