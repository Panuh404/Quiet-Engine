#version 450

layout(push_constant) uniform StaticMeshData
{
    mat4 Model;
    vec4 MaterialBaseColor;
    vec4 LightDirectionIntensity;
    vec4 LightColor;
    vec4 AmbientColorIntensity;
} pc;

layout(location = 0) in vec4 vMaterialColor;
layout(location = 1) in vec3 vWorldNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(vWorldNormal);

    vec3 rawLightDirection = pc.LightDirectionIntensity.xyz;
    float lightLength = length(rawLightDirection);
    vec3 lightDirection =
    lightLength > 0.0001
    ? rawLightDirection / lightLength
    : vec3(0.0, 1.0, 0.0);

    float lightIntensity = pc.LightDirectionIntensity.w;

    vec3 ambient = pc.AmbientColorIntensity.rgb * pc.AmbientColorIntensity.w;
    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 direct = pc.LightColor.rgb * diffuse * lightIntensity;
    vec3 litColor = vMaterialColor.rgb * clamp(ambient + direct, 0.0, 1.0);

    outColor = vec4(litColor, vMaterialColor.a);
}