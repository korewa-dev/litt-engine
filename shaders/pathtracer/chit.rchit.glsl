#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0) uniform SceneConstants {
    uvec2 uResolution;
    uint uMaxBounces;
    uint uFrameCount;
    vec3 vCameraPos;
    float fCameraYaw;
    float fCameraPitch;
    float fFov;
    float fAspect;
    uint uLightCount;
    uint uTriangleCount;
    uint uSphereCount;
    uint uMaterialCount;
    vec3 vBoundsMin;
    vec3 vBoundsMax;
    uint uPad[8];
} uSceneConstants;

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    struct {
        vec3 albedo;
        float roughness;
        float metallic;
        float ior;
        vec3 emissive;
        float light_intensity;
        uint pad[3];
    } materials[];
} uMaterials;

layout(set = 0, binding = 4) readonly buffer LightBuffer {
    struct {
        vec3 position;
        vec3 color;
        float intensity;
        float radius;
    } lights[];
} uLights;

layout(location = 0) rayPayloadEXT vec3 rpColor;
layout(location = 1) rayPayloadEXT float rpThroughput;
layout(location = 2) rayPayloadEXT int rpDepth;
layout(location = 0) hitAttributeEXT vec3 haAttrib;

// Hash RNG
uint hash(uint n) {
    n = (n ^ 61u) ^ (n >> 16u);
    n *= 9u;
    n = n ^ (n >> 4u);
    n *= 0x27d4eb2du;
    return n ^ (n >> 15u);
}

uint rand_u32(inout uint rng) { return hash(rng); }
float rand_f32(inout uint rng) { return float(rand_u32(rng)) / 4294967295.0; }

vec3 fresnel_schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnel_schlick_roughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    uint rng = uint(gl_PrimitiveIndex) * 6282799u + uint(gl_InstanceCustomIndexEXT) * 192703u;
    
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT.glHitAttributeEXT * gl_WorldRayDirectionEXT;
    vec3 normal = normalize(haAttrib);
    
    // Get material
    uint mat_idx = gl_InstanceID * 6u;
    vec3 albedo = uMaterials.materials[mat_idx].albedo;
    float roughness = uMaterials.materials[mat_idx].roughness;
    float metallic = uMaterials.materials[mat_idx].metallic;
    float ior = uMaterials.materials[mat_idx].ior;
    vec3 emissive = uMaterials.materials[mat_idx].emissive;
    float light_intensity = uMaterials.materials[mat_idx].light_intensity;
    
    // Emit light
    if (light_intensity > 0.0) {
        rpColor = emissive * light_intensity;
        rpThroughput = 1.0;
        return;
    }
    
    // Shade with direct lighting
    vec3 wo = normalize(gl_WorldRayOriginEXT - hitPoint);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 f = vec3(1.0);
    vec3 color = vec3(0.0);
    
    // Sample lights
    for (uint i = 0u; i < uSceneConstants.uLightCount; i++) {
        vec3 lightPos = uLights.lights[i * 4u].position;
        vec3 lightColor = uLights.lights[i * 4u].color;
        float lightIntensity = uLights.lights[i * 4u].intensity;
        float lightRadius = uLights.lights[i * 4u].radius;
        
        vec3 toLight = lightPos - hitPoint;
        float dist = length(toLight);
        vec3 lightDir = normalize(toLight);
        
        // Shadow test
        traceRayEXT(uInstances.instances, 0x1u, 0xFFu, 0u, 0u, 0u,
                    hitPoint + normal * 0.001, 1e-5, dist, lightDir, 0u);
        
        float NdotL = max(dot(normal, lightDir), 0.0);
        if (NdotL > 0.0) {
            float cosTheta = dot(normal, lightDir);
            vec3 radiance = lightColor * lightIntensity / (dist * dist);
            
            // GGX BRDF
            vec3 F = fresnel_schlick_roughness(cosTheta, F0, roughness);
            float NDF = roughness * roughness / (3.14159 * (cosTheta * cosTheta * (roughness * roughness - 1.0) + 1.0));
            float G = 1.0; // Simplified
            float pdf = NDF * cosTheta / (4.0 * max(dot(wo, normal), 0.0));
            
            color += radiance * albedo * NDF * G / max(pdf, 0.001);
            f = F;
        }
    }
    
    // Russian roulette termination
    float p = max(max(color.r, color.g), color.b);
    if (rpDepth > 3 && rand_f32(rng) > p) {
        rpColor = color;
        rpThroughput = 0.0;
        return;
    }
    
    rpColor = color;
    rpThroughput = rpThroughput * max(p, 0.001);
    rpDepth++;
}
