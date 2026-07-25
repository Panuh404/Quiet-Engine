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

    // x = minor spacing
    // y = major spacing
    // z = fade start
    // w = fade end
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
    const float minorSpacing = max(pc.Parameters.x, 0.0001);
    const float majorSpacing = max(pc.Parameters.y, minorSpacing);

    const float minorMask = GridLineMask(worldPosition.xz, minorSpacing);
    const float majorMask = GridLineMask(worldPosition.xz, majorSpacing);

    vec3 gridColor = pc.MinorColor.rgb;
    float gridAlpha = minorMask * pc.MinorColor.a;

    gridColor = mix(gridColor, pc.MajorColor.rgb, majorMask);
    gridAlpha = max(gridAlpha, majorMask * pc.MajorColor.a);

    // The X axis extends along world X, therefore its
    // distance from the axis is world Z.
    const float xAxisMask = AxisLineMask(worldPosition.z);
    gridColor = mix(gridColor, pc.XAxisColor.rgb, xAxisMask);
    gridAlpha = max(gridAlpha, xAxisMask * pc.XAxisColor.a);

    // The Z axis extends along world Z, therefore its
    // distance from the axis is world X.
    const float zAxisMask = AxisLineMask(worldPosition.x);
    gridColor = mix(gridColor, pc.ZAxisColor.rgb, zAxisMask);
    gridAlpha = max(gridAlpha, zAxisMask * pc.ZAxisColor.a);

    const float fadeStart = pc.Parameters.z;
    const float fadeEnd = max(pc.Parameters.w, fadeStart + 0.0001);

    const float cameraDistance = length(worldPosition - vCameraPosition);
    const float distanceFade = 1.0 - clamp((cameraDistance - fadeStart) / (fadeEnd - fadeStart), 0.0, 1.0);

    gridAlpha *= distanceFade;
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

    // Push the grid very slightly behind coplanar mesh surfaces.
    gl_FragDepth = min(fragmentDepth + 0.00001, 1.0);
    outColor = vec4(gridColor, gridAlpha);
}