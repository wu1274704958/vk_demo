#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inTangent;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec2 outUV;
layout (location = 5) out vec3 outTangent;

void main() 
{
	outNormal = inNormal;
	outColor = inColor;
	gl_Position = ubo.projection * ubo.model * vec4(inPos, 1.0);

	vec4 pos = ubo.model * vec4(inPos, 1.0);
	outNormal = mat3(ubo.model) * inNormal;

	vec3 lightPos = vec3(1.0f, 1.0f, 1.0f);
	outLightVec = lightPos.xyz - pos.xyz;
	outViewVec = -pos.xyz;
	outUV = inUV;
	outTangent = inTangent;
}
