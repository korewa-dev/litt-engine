#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 rpColor;
layout(location = 1) rayPayloadEXT float rpThroughput;
layout(location = 2) rayPayloadEXT int rpDepth;

void main() {
    // Sky gradient
    float t = max(gl_WorldRayDirectionEXT.y, 0.0);
    vec3 sky = mix(vec3(0.5, 0.7, 1.0), vec3(1.0, 1.0, 1.0), t);
    rpColor += sky * rpThroughput;
}
