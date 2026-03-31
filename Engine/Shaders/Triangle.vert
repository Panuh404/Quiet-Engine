#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform TransformData
{
    mat4 MVP;
} pc;

layout(location = 0) out vec3 vColor;

void main()
{
    gl_Position = pc.MVP * vec4(inPosition, 1.0);
    vColor = inColor;
}