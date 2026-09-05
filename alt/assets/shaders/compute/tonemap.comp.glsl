#version 450
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uInput;
layout(set = 0, binding = 1, scalar) uniform Constants {
    float fExposure;
    float fContrast;
    float fGamma;
    uint uPad;
    uint uWidth;
    uint uHeight;
} uConstants;

layout(rgba8unorm, set = 0, binding = 2) uniform image2D uOutput;

vec3 aces_tonemap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uConstants.uWidth) || pos.y >= int(uConstants.uHeight)) return;
    
    vec3 color = texture(uInput, (vec2(pos) + 0.5) / vec2(uConstants.uWidth, uConstants.uHeight)).rgb;
    color *= uConstants.fExposure;
    color = aces_tonemap(color);
    color = pow(color, vec3(1.0 / uConstants.fGamma));
    
    // Apply contrast
    color = (color - 0.5) * uConstants.fContrast + 0.5;
    
    imageStore(uOutput, pos, vec4(color, 1.0));
}
