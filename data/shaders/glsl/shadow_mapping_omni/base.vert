#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;

layout (set = 0,binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 lightSpace;
    vec4 viewPos;
    vec4 lightPos;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outViewPos;
layout (location = 2) out vec3 outLightPos;
layout (location = 3) out vec2 outUV;
layout (location = 4) out vec3 outFragPos;
layout (location = 5) out vec3 outColor;


void main()
{
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos, 1.0);
    mat3 mmat = mat3(ubo.model);

    outNormal = normalize(mmat * inNormal);
    outViewPos = ubo.viewPos.xyz;
    outLightPos = ubo.lightPos.xyz;
    outUV = inUV;
    outFragPos = (ubo.model * vec4(inPos, 1.0)).xyz;
    outColor = inColor;
}