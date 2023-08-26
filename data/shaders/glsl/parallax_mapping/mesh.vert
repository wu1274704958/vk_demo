#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inTangent;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	vec4 viewPos;
	vec4 lightPos;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outTViewPos;
layout (location = 3) out vec3 outTLightPos;
layout (location = 4) out vec2 outUV;
layout (location = 5) out vec3 outFragPos;

void main() 
{
	outNormal = inNormal;
	outColor = inColor;
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos, 1.0);

	vec4 pos = ubo.model * vec4(inPos, 1.0);
	outNormal = normalize(mat3(ubo.model) * inNormal);
	vec3 T = normalize(mat3(ubo.model) * inTangent);
	vec3 B = normalize(cross(outNormal,T));
	mat3 TBN = transpose(mat3(T,B,outNormal));


	outTLightPos = TBN * ubo.lightPos.xyz;
	outTViewPos =  TBN * ubo.viewPos.xyz;
	outFragPos = TBN * pos.xyz;
	outUV = inUV;

}
