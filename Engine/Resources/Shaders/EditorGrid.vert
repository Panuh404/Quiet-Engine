#version 450

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 View;
    mat4 Projection;
} ubo;

layout(location = 0) out vec3 vNearPoint;
layout(location = 1) out vec3 vFarPoint;
layout(location = 2) flat out vec3 vCameraPosition;

const vec2 FullscreenTriangle[3] =
{
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
};

void main()
{
    const vec2 position = FullscreenTriangle[gl_VertexIndex];
    gl_Position = vec4(position, 0.0, 1.0);

    const mat4 inverseViewProjection = inverse(ubo.Projection * ubo.View);

    vec4 nearPoint = inverseViewProjection * vec4(position, 0.0, 1.0);
    vec4 farPoint = inverseViewProjection * vec4(position, 1.0, 1.0);

    vNearPoint = nearPoint.xyz / nearPoint.w;
    vFarPoint = farPoint.xyz / farPoint.w;
    vCameraPosition = inverse(ubo.View)[3].xyz;
}