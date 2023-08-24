#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inTViewPos;
layout (location = 3) in vec3 inTLightPos;
layout (location = 4) in vec2 inUV;
layout (location = 5) in vec3 inTPos;

layout (location = 0) out vec4 outFragColor;
layout (binding = 1) uniform sampler2D hight_map;
layout (binding = 2) uniform sampler2D diffuse_tex;

layout (binding = 3) uniform MappingArgs
{
	int mappingMode;
	float heightScale;
	float numLayers;
	float bias;
} mappingArgs;


vec2 parallax_mapping(vec3 v)
{
	float h = 1.0 - textureLod(hight_map, inUV, 0.0).a;
	vec2 p = v.xy * (h * (mappingArgs.heightScale * 0.5) + mappingArgs.bias) / v.z;
	return inUV - p;
}

vec2 step_parallax_mapping(vec3 v)
{
	float depthDelta = 1.0f / mappingArgs.numLayers;
	vec2 uvDelta = v.xy * mappingArgs.heightScale / ( v.z * mappingArgs.numLayers);
	float currLayerDepth = 0;
	vec2 currUV = inUV;
	for(int i = 0;i < mappingArgs.numLayers;++i)
	{
		currUV -= uvDelta;
		currLayerDepth += depthDelta;
		float h = 1.0f - textureLod(hight_map, currUV, 0.0f).a;
		if(h < currLayerDepth)
		{
			break;
		}
	}
	return currUV;
}

vec2 parallax_occlusion_mapping(vec3 v)
{
	float depthDelta = 1.0f / mappingArgs.numLayers;
	vec2 uvDelta = v.xy * mappingArgs.heightScale / ( v.z * mappingArgs.numLayers);
	float currLayerDepth = 0;
	vec2 currUV = inUV;
	float currHeight = 0.0f;
	for(int i = 0;i < mappingArgs.numLayers;++i)
	{
		currUV -= uvDelta;
		currLayerDepth += depthDelta;
		currHeight = 1.0f - textureLod(hight_map, currUV, 0.0f).a;
		if(currHeight < currLayerDepth)
		{
			break;
		}
	}
	vec2 prevUV = currUV + uvDelta;
	float nextDepth = currHeight - currLayerDepth;
	float prevDepth = (1.0f - textureLod(hight_map, currUV, 0.0f).a) - (currLayerDepth - depthDelta);
	float weight = nextDepth / (nextDepth - prevDepth);
	return mix(currUV,prevUV,weight);
}

void main()
{
	vec3 V = normalize(inTViewPos - inTPos);
	vec2 uv = inUV;

	switch(mappingArgs.mappingMode)
	{
			case 1:
			uv = parallax_mapping(V);
			break;
			case 2:
			uv = step_parallax_mapping(V);
			break;
			case 3:
			uv = parallax_occlusion_mapping(V);
			break;
	}

	if(uv.x > 1.0f || uv.x < 0.0f || uv.y > 1.0f || uv.y < 0.0f) {
		discard;
	}

	vec3 N = normalize(textureLod(hight_map,uv,0.0f).rgb * 2.0f - 1.0f);// mat3(inTangent,B,inNormal) * Nft;
	vec3 L = normalize(inTLightPos - inTPos);
	vec3 H = normalize(L + V);
	vec3 R = reflect(-L, N);

	vec3 color = texture(diffuse_tex,uv).rgb;
	vec3 ambient = 0.1 * color;
	vec3 diffuse = max(dot(N, L), 0.0) * color;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
	outFragColor = vec4((ambient + diffuse) * inColor.rgb + specular, 1.0);
}