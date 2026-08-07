#version 450

layout(set = 0, binding = 0) uniform sampler2D uSelectionMask;

layout(push_constant) uniform SelectionOutlineData
{
    vec4 OutlineColor;
    // x = outline radius in pixels
    // yzw = reserved
    vec4 Parameters;
} pc;

layout(location = 0) out vec4 outColor;

void main()
{
    const ivec2 imageSize = textureSize(uSelectionMask, 0);
    const ivec2 maximumPixel = imageSize - ivec2(1);
    const ivec2 pixel = clamp(ivec2(gl_FragCoord.xy), ivec2(0), maximumPixel);
    const float centerMask = texelFetch(uSelectionMask, pixel, 0).r;

    // Draw only outside the selected geometry.
    if (centerMask >= 0.5)
        discard;

    const int radius = clamp(int(round(pc.Parameters.x)), 1, 4);
    float neighboringMask = 0.0;

    for (int y = -4; y <= 4; ++y)
    {
        for (int x = -4; x <= 4; ++x)
        {
            if (x == 0 && y == 0)
                continue;

            if (abs(x) > radius || abs(y) > radius)
                continue;

            const ivec2 samplePixel = clamp(pixel + ivec2(x, y), ivec2(0), maximumPixel);
            neighboringMask = max( neighboringMask, texelFetch(uSelectionMask, samplePixel, 0).r);
        }
    }

    if (neighboringMask < 0.5)
        discard;

    outColor = pc.OutlineColor;
}