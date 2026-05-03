#version 450

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 View;
    mat4 Projection;
} ubo;

layout(push_constant) uniform StaticMeshData
{
    mat4 Model;
    vec4 MaterialBaseColor;
    vec4 LightDirectionIntensity;
    vec4 LightColor;
    vec4 AmbientColorIntensity;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec4 vMaterialColor;
layout(location = 1) out vec3 vWorldNormal;

void main()
{
    gl_Position = ubo.Projection * ubo.View * pc.Model * vec4(inPosition, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(pc.Model)));
    vWorldNormal = normalize(normalMatrix * inNormal);

    vMaterialColor = pc.MaterialBaseColor;
}