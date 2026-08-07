#version 450

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 View;
    mat4 Projection;
} ubo;

layout(push_constant) uniform SelectionMaskData
{
    mat4 Model;
} pc;

layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = ubo.Projection * ubo.View * pc.Model * vec4(inPosition, 1.0);
}