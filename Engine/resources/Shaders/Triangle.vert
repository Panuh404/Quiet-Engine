#version 450

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 View;
    mat4 Projection;
} ubo;

layout(push_constant) uniform TransformData
{
    mat4 Model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vColor;

void main()
{
    gl_Position = ubo.Projection * ubo.View * pc.Model * vec4(inPosition, 1.0);
    vColor = inColor;
}