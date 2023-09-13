#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 2) in vec3 inViewPos;
layout (location = 3) in vec3 inLightPos;
layout (location = 4) in vec2 inUV;
layout (location = 5) in vec3 inFragPos;
layout (location = 6) in vec4 inLightSpacePos;
layout (location = 7) in vec3 inColor;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform sampler2D shadowMap;
layout (set = 0, binding = 2) uniform SpotLightData {
    vec3 direct;
    float phi;
    float theta;
    float range;
    float sphereCustomNormal;
} spotLight;
layout (push_constant) uniform PushConstants
{
    float isCustomNormal;
} push;

const float MaxBias = 0.001f;

#define ambient 0.3

float textureProj(vec4 shadowCoord, vec2 off)
{
    float dist = texture( shadowMap, shadowCoord.st + off ).r;
    vec3 N = normalize(inNormal);
    vec3 L = normalize(inLightPos);
    if (shadowCoord.x >= 0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0 && shadowCoord.y <= 1.0 && shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
    {
        float bias =  (1.0f - max(dot(N, L),0.0f)) * MaxBias;
        if (shadowCoord.w > 0.0 && dist < shadowCoord.z - bias)
        {
            return 0.0f;
        }
    }
    return 1.0f;
}

float filterPCF(vec4 shadowCoord)
{
    vec2 size = textureSize(shadowMap, 0);
    vec2 delat = 1.0f / size;
    float shadowFactor = 0.0f;
    int range = 1;
    for(int y = -range; y <= range; y++)
    {
        for(int x = -range; x <= range; x++)
        {
            shadowFactor += textureProj(shadowCoord, vec2(x * delat.x, y * delat.y));
        }
    }
    float a = (1.0f + range * 2.0f);
    return shadowFactor / (a * a);
}

vec2 calcSpotLight()
{
    float dist = length(inLightPos - inFragPos);
//    vec3 FL_dir = normalize(inFragPos - inLightPos);
//    vec3 L_dir = normalize(spotLight.direct);
//    float theta = acos(dot(L_dir, FL_dir));
    float strengthForRange = max(0.0f,1.0 - pow(dist / spotLight.range,2.0f));
//    if(theta > spotLight.phi)
//        return vec2(0.0f);
    //float strength = theta > spotLight.theta ? 1.0 - smoothstep(spotLight.theta, spotLight.phi, theta)  : 1.0f;
    return vec2(1.0f,  strengthForRange );
}

void main()
{
    vec3 N = normalize(inNormal);
    if(push.isCustomNormal > 0.0f)
    {
        N = normalize(-inFragPos);
    }
    vec3 L = normalize(inLightPos - inFragPos);
    vec3 V = normalize(inViewPos - inFragPos);
    vec3 R = normalize(reflect(-L, N));

    //float shadow = textureProj(inLightSpacePos / inLightSpacePos.w, vec2(0.0));
    float shadow = filterPCF(inLightSpacePos / inLightSpacePos.w);

    vec2 sp = calcSpotLight();

    vec3 diffuse = inColor * max(0.0f,dot(L, N)) * 0.5f;
    vec3 specular = pow(max(dot(R, V), 0.0), 64.0) * vec3(0.2f) * inColor;
    vec3 c = vec3(ambient) + (diffuse + specular) * sp.y;
    outColor = vec4( c ,1.0f);
    //outColor = vec4(inLightSpacePos.xy / inLightSpacePos.w,0.0f,1.0f);
    //outColor = vec4(texture(shadowMap,(inLightSpacePos.xy / inLightSpacePos.w)).rrr,1.0f);
    //outColor = vec4(texture(shadowMap,inUV).rrr,1.0f);
}