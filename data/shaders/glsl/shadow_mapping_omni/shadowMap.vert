#version 450

layout (location = 0) in vec3 inPos;
layout (set = 0,binding = 0) uniform UBO
{
    mat4 mat;
    vec4 lightPos;
} ubo;
layout (push_constant) uniform PushConstants
{
    mat4 vp;
} push;

layout (location = 0) out vec4 outPos;
layout (location = 1) out vec3 outLightPos;

void main()
{
    gl_Position = push.vp * ubo.mat * vec4(inPos, 1.0);
    outPos = ubo.mat * vec4(inPos, 1.0);
    outLightPos = ubo.lightPos.xyz;
}