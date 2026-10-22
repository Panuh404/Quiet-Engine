#version 450

layout(push_constant) uniform PanelData
{
    layout(offset = 0)  vec4 Rect;
    layout(offset = 16) vec4 Color;
    layout(offset = 32) vec4 Viewport;
} pc;

layout(location = 0) out vec4 outColor;

const vec2 kCorners[6] =
{
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),

    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
};

void main()
{
    const vec2 corner = kCorners[gl_VertexIndex];
    const vec2 pixelPosition = mix(pc.Rect.xy, pc.Rect.zw, corner);

    // A positive-height Vulkan viewport maps NDC Y=-1 to its top edge.
    const vec2 ndcPosition =
    pixelPosition / pc.Viewport.xy * 2.0 - 1.0;

    gl_Position = vec4(ndcPosition, 0.0, 1.0);
    outColor = pc.Color;
}