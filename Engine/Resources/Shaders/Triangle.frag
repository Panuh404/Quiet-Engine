#version 450

layout(push_constant) uniform TransformData
{
    mat4 Model;
    vec4 BaseColor;
    vec4 LightDirectionIntensity;
    vec4 AmbientColorIntensity;
} pc;

layout(location = 0) in vec4 vBaseColor;
layout(location = 1) in vec3 vWorldNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(vWorldNormal);

    vec3 lightDirection = normalize(pc.LightDirectionIntensity.xyz);
    float lightIntensity = pc.LightDirectionIntensity.w;

    vec3 ambient = pc.AmbientColorIntensity.rgb * pc.AmbientColorIntensity.w;
    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 litColor = vBaseColor.rgb * clamp(ambient + diffuse * lightIntensity, 0.0, 1.0);

    outColor = vec4(litColor, vBaseColor.a);
}