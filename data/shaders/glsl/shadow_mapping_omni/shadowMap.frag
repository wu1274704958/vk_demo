#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec3 inLightPos;

layout (location = 0) out float Out;
void main()
{
    Out = length(inPos.xyz - inLightPos);
}