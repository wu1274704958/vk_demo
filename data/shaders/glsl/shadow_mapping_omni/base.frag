#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inViewPos;
layout (location = 2) in vec3 inLightPos;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inFragPos;
layout (location = 5) in vec3 inColor;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform samplerCube shadowMap;
layout (set = 0, binding = 2) uniform SpotLightData {
    vec3 direct;
    float phi;
    float theta;
    float range;
} spotLight;
layout (push_constant) uniform PushConstants
{
    float isCustomNormal;
} push;

const float MaxBias = 0.2f;

#define ambient 0.3

float textureProj(vec3 off)
{
    vec3 shadowCoord = (inFragPos + off) - inLightPos;
    float sampleDist = texture( shadowMap, shadowCoord).r;
    float dist = length(shadowCoord);
//    vec3 N = normalize(inNormal);
//    if(push.isCustomNormal > 0.0f)
//    {
//        N = normalize(-(inFragPos + off));
//    }
//    vec3 L = normalize(inLightPos - (inFragPos + off));

    float bias = /*(1.0f - max(dot(N, L),0.0f)) */ MaxBias;
    sampleDist += bias;
    if (dist <= sampleDist )
    {
        return 1.0f;
    }
    return 0.0f;
}

const vec3 sampleOffsetDirections[20] = vec3[]
(
vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

float filterPCF()
{
    float shadow = 0.0;
    int samples = 20;
    float diskRadius = 0.03;
    for(int i = 0; i < samples; ++i)
    {
        shadow += textureProj(sampleOffsetDirections[i] * diskRadius);
    }
    return shadow /= float(samples);
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


    //float shadow = textureProj(vec3(0));
    float shadow = filterPCF();
    vec2 sp = calcSpotLight();

    vec3 diffuse = inColor * max(0.0f,dot(L, N)) * 0.5f;
    vec3 specular = pow(max(dot(R, V), 0.0), 64.0) * vec3(0.2f) * inColor;
    vec3 c = vec3(ambient) + (diffuse + specular) * sp.y * shadow;
//    if(push.isCustomNormal > 0.0f)
//    {
//        float d = texture(shadowMap, normalize(inFragPos)).r / 20;
//        c = vec3(d,d,d);
//    }
    outColor = vec4( c,1.0f);
    //outColor = vec4(inLightSpacePos.xy / inLightSpacePos.w,0.0f,1.0f);
    //outColor = vec4(texture(shadowMap,(inLightSpacePos.xy / inLightSpacePos.w)).rrr,1.0f);
    //outColor = vec4(texture(shadowMap,inUV).rrr,1.0f);
}