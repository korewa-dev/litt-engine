#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

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

layout(set = 0, binding = 1) readonly buffer TriangleBuffer {
    struct {
        vec3 v0;
        vec3 v1;
        vec3 v2;
        vec3 normal;
        uint material_id;
        uint pad[3];
    } triangles[];
} uTriangles;

layout(set = 0, binding = 2) readonly buffer SphereBuffer {
    struct {
        vec3 center;
        float radius;
        uint material_id;
        uint pad[3];
    } spheres[];
} uSpheres;

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

layout(set = 0, binding = 5) readonly buffer InstanceBuffer {
    struct {
        mat3x4 transform;
        uint instanceMask;
        uint instanceID;
        uint sbtOffset;
        uint flags;
    } instances[];
} uInstances;

layout(set = 0, binding = 6) uniform sampler2D uAccumulationBuffer;
layout(set = 0, binding = 7) uniform sampler2D uVelocityBuffer;
layout(set = 0, binding = 8) uniform sampler2D uOutputBuffer;

layout(location = 0) rayPayloadEXT vec3 rpColor;
layout(location = 1) rayPayloadEXT float rpThroughput;
layout(location = 2) rayPayloadEXT int rpDepth;
layout(location = 0) hitAttributeEXT vec3 haAttrib;

layout(push_constant) uniform PushConstants {
    uint uFrameCount;
    uint uMaxBounces;
    uint uResolution;
    uint uPad;
    vec3 vCameraPos;
    float fCameraYaw;
    float fCameraPitch;
    float fPad;
    vec3 vLightPos;
    vec3 vLightColor;
    float fLightIntensity;
} uPushConstants;

layout(rgba32f, set = 0, binding = 9) uniform image2D uOutputImage;

// Simple hash-based RNG
uint hash(uint n) {
    n = (n ^ 61u) ^ (n >> 16u);
    n *= 9u;
    n = n ^ (n >> 4u);
    n *= 0x27d4eb2du;
    return n ^ (n >> 15u);
}

uint rand_u32(inout uint rng) {
    rng = hash(rng);
    return rng;
}

float rand_f32(inout uint rng) {
    return float(rand_u32(rng)) / 4294967295.0;
}

vec3 random_hemisphere(vec3 N, inout uint rng) {
    float r1 = rand_f32(rng);
    float r2 = rand_f32(rng);
    float phi = 2.0 * 3.141592653589793 * r1;
    float cosTheta = sqrt(r2);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(N, up));
    vec3 B = cross(N, T);
    
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return normalize(T * H.x + B * H.y + N * H.z);
}

vec3 sample_sphere(vec3 origin, vec3 dir, vec3 center, float radius, inout uint rng) {
    vec3 oc = origin - center;
    float a = dot(dir, dir);
    float b = 2.0 * dot(oc, dir);
    float c = dot(oc, oc) - radius * radius;
    float t = (-b - sqrt(b * b - 4.0 * a * c)) / (2.0 * a);
    return vec3(t, 0.0, 0.0);
}

void main() {
    uint seed = uSceneConstants.uFrameCount * 1000u + gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * 192703u;
    uint rng = seed;
    
    vec2 uv = (vec2(gl_LaunchIDEXT.xy) + vec2(rand_f32(rng))) / uSceneConstants.uResolution;
    
    float aspect = uSceneConstants.fAspect;
    float fov = uSceneConstants.fFov;
    float scale = tan(fov * 0.5);
    
    vec3 rayOrigin = uSceneConstants.vCameraPos;
    vec3 rayDir = normalize(vec3(
        (uv.x * 2.0 - 1.0) * aspect * scale,
        (1.0 - uv.y * 2.0) * scale,
        -1.0
    ));
    
    // Apply camera rotation
    float cy = cos(uSceneConstants.fCameraYaw);
    float sy = sin(uSceneConstants.fCameraYaw);
    float cp = cos(uSceneConstants.fCameraPitch);
    float sp = sin(uSceneConstants.fCameraPitch);
    rayDir = vec3(
        rayDir.x * cy + rayDir.z * sy,
        rayDir.y * cp + rayDir.x * (-sp) * sy + rayDir.z * sp * cy,
        rayDir.z * cy - rayDir.x * sy
    );
    rayDir = normalize(rayDir);
    
    rpColor = vec3(0.0);
    rpThroughput = 1.0;
    rpDepth = 0;
    
    // Trace the ray
    uint hit_any = 0;
    uint hit_mat_id = 0;
    vec3 hit_point = vec3(0.0);
    vec3 hit_normal = vec3(0.0);
    float hit_t = 1e20;
    
    // Test spheres first
    for (uint i = 0u; i < uSceneConstants.uSphereCount; i++) {
        uint idx = i * 8u;
        vec3 center = uSpheres.spheres[idx].center;
        float radius = uSpheres.spheres[idx].radius;
        vec3 oc = rayOrigin - center;
        float a = dot(rayDir, rayDir);
        float b = 2.0 * dot(oc, rayDir);
        float c = dot(oc, oc) - radius * radius;
        float disc = b * b - 4.0 * a * c;
        if (disc > 0.0) {
            float t = (-b - sqrt(disc)) / (2.0 * a);
            if (t > 1e-5 && t < hit_t) {
                hit_t = t;
                hit_point = rayOrigin + rayDir * t;
                hit_normal = (hit_point - center) / radius;
                hit_mat_id = uSpheres.spheres[idx].material_id;
                hit_any = 1u;
            }
        }
    }
    
    // Trace triangles
    traceRayEXT(uInstances.instances, 0x1u, 0xFFu, 0u, 0u, 0u, rayOrigin, 1e-5, 1e20, rayDir, 0u);
    
    // Accumulate result
    vec3 accumulated = texture(uAccumulationBuffer, uv).rgb;
    float alpha = 1.0 / float(uSceneConstants.uFrameCount + 1u);
    rpColor = mix(accumulated, rpColor, alpha);
    
    imageStore(uOutputImage, ivec2(gl_LaunchIDEXT.xy), vec4(rpColor, 1.0));
}
