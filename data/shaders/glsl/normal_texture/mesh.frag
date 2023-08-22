#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec2 inUV;
layout (location = 5) in vec3 inTangent;

layout (location = 0) out vec4 outFragColor;
layout (binding = 1) uniform sampler2D normal_map;
layout (binding = 2) uniform sampler2D diffuse_tex;

void main() 
{
	vec3 Nft = texture(normal_map, inUV).xyz * 2.0f - 1.0f;
	vec3 B = normalize( cross(inNormal,inTangent) );

	vec3 N = mat3(inTangent,B,inNormal) * Nft;
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 ambient = vec3(0.1);
	vec3 diffuse = max(dot(N, L), 0.0) * texture(diffuse_tex, inUV).xyz;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
	outFragColor = vec4((ambient + diffuse) * inColor.rgb + specular, 1.0);
}