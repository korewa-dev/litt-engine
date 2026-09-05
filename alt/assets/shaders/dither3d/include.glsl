// Dither3D - Surface-Stable Fractal Dithering for Litt Engine
// Ported from Dither3DInclude.cginc (MPL v2.0, Rune Skovbo Johansen)
//
// Core algorithm:
//   1. Compute surface UV rate-of-change via derivatives (dFdx/dFdy)
//   2. SVD of Jacobian to get min/max spatial frequency
//   3. Map frequency to dot spacing (surface-stable)
//   4. Look up fractal dither pattern from 3D texture
//   5. Adjust dot size/contrast based on surface brightness

uniform sampler3D uDitherTex;
uniform sampler2D uDitherRampTex;
uniform float uDitherTexWidth;   // X resolution of 3D texture (e.g., 128 for 8x8 pattern)
uniform mat4 uProj;               // Projection matrix for radial compensation

uniform float uDitherScale;         // Dot scale (default 5.0)
uniform float uDitherSizeVariability; // 0=Bayer-style, 1=Halftone-style (default 0.0)
uniform float uDitherContrast;      // Dot contrast (default 1.0)
uniform float uDitherStretchSmooth; // Stretch smoothness (default 1.0)
uniform float uDitherInputExposure; // Input brightness exposure (default 1.0)
uniform float uDitherInputOffset;   // Input brightness offset (default 0.0)

// Color mode: 0=Grayscale, 1=RGB, 2=CMYK
uniform int uDitherColorMode;

// Display mode: 0=Normal, 1=DebugFractal
uniform int uDitherDebugMode;

// Inverse dots mode
uniform bool uDitherInverseDots;

// Compute brightness from color
float getBrightness(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

// Convert RGB to CMYK
vec4 rgbToCmyk(vec3 rgb) {
    float k = min(1.0 - rgb.r, min(1.0 - rgb.g, 1.0 - rgb.b));
    float invK = 1.0 - k;
    vec3 cmy = vec3(0.0);
    if (invK > 0.001) {
        cmy.r = (1.0 - rgb.r - k) / invK;
        cmy.g = (1.0 - rgb.g - k) / invK;
        cmy.b = (1.0 - rgb.b - k) / invK;
    }
    return vec4(cmy, k);
}

// Convert CMYK to RGB
vec3 cmykToRgb(vec4 cmyk) {
    float k = cmyk.a;
    float invK = 1.0 - k;
    return vec3(
        1.0 - min(1.0, cmyk.r * invK + k),
        1.0 - min(1.0, cmyk.g * invK + k),
        1.0 - min(1.0, cmyk.b * invK + k)
    );
}

// Rotate UV coordinates by a unit direction vector
vec2 rotateUV(vec2 uv, vec2 xUnitDir) {
    return uv.x * xUnitDir + uv.y * vec2(-xUnitDir.y, xUnitDir.x);
}

/**
 * Core dithering function.
 *
 * @param uv        Surface UV coordinates for dither texture lookup
 * @param screenPos Clip-space position (for derivative computation)
 * @param color     Input color (RGB)
 * @return          Dithered color
 */
vec3 applyDither3D(vec2 uv, vec4 screenPos, vec3 color) {
    // Apply input exposure/offset to brightness
    color = clamp(color * uDitherInputExposure + uDitherInputOffset, 0.0, 1.0);

    // Compute derivatives of UV coordinates
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);

    // Texture resolution info
    float xRes = uDitherTexWidth;
    float invXres = 1.0 / xRes;

    // dotsPerSide = xRes / 16 (hardcoded relationship from Dither3D texture generator)
    // For 128x128 texture: dotsPerSide = 8 (8x8 pattern)
    float dotsPerSide = xRes / 16.0;
    float dotsTotal = dotsPerSide * dotsPerSide; // Z resolution (number of layers)
    float invZres = 1.0 / dotsTotal;

    // Brightness curve lookup
    // This ensures dither output has correct brightness at different input levels
    float brightness = getBrightness(color);
    vec2 lookup = vec2((0.5 * invXres + (1.0 - invXres) * brightness), 0.5);
    float brightnessCurve = texture(uDitherRampTex, lookup).r;

    #ifdef DITHER_INVERSE
        brightness = 1.0 - brightness;
    #endif

    // Radial compensation (keep dots stable under camera rotation)
    #ifdef DITHER_RADIAL
        vec2 screenP = (screenPos.xy / screenPos.w - 0.5) * 2.0;
        vec2 viewDirProj = vec2(
            screenP.x / uProj[0][0],
            screenP.y / -uProj[1][1]
        );
        float radialCompensation = dot(viewDirProj, viewDirProj) + 1.0;
        dx *= radialCompensation;
        dy *= radialCompensation;
    #endif

    // SVD of Jacobian to get min/max spatial frequency
    // This is more accurate than fwidth and handles arbitrary surface angles
    float Q = dot(vec4(dx, dy), vec4(dx, dy));
    float R = dx.x * dy.y - dx.y * dy.x; // determinant
    float discriminantSqr = max(0.0, Q * Q - 4.0 * R * R);
    float discriminant = sqrt(discriminantSqr);
    vec2 freq = sqrt(vec2(Q + discriminant, Q - discriminant) / 2.0);

    // Dot spacing from minimum frequency (largest stretching direction)
    float spacing = freq.y;
    float scaleExp = exp2(uDitherScale);
    spacing *= scaleExp;
    spacing *= dotsPerSide * 0.125;

    // Brightness-spacing multiplier
    // 0 = Bayer-style (shading controls dot count)
    // 1 = Halftone-style (shading controls dot sizes)
    float brightnessSpacingMultiplier =
        pow(brightnessCurve * 2.0 + 0.001, -(1.0 - uDitherSizeVariability));
    spacing *= brightnessSpacingMultiplier;

    // Fractal level selection
    float spacingLog = log2(spacing);
    int patternScaleLevel = int(floor(spacingLog));
    float f = spacingLog - float(patternScaleLevel);

    // UV coordinates at current fractal level
    vec2 uvScaled = uv / exp2(float(patternScaleLevel));

    // Sub-layer for interpolation between pattern densities
    float subLayer = mix(0.25 * dotsTotal, dotsTotal, 1.0 - f);

    #ifdef DITHER_QUANTIZE
        float origSubLayer = subLayer;
        subLayer = floor(subLayer + 0.5);
        float thresholdTweak = sqrt(subLayer / origSubLayer);
    #endif

    // Normalize subLayer to [0,1] range for 3D texture lookup
    subLayer = (subLayer - 0.5) * invZres;

    // Sample 3D dither texture
    float pattern = texture(uDitherTex, vec3(uvScaled, subLayer)).r;

    // Dot size calculation
    float threshold = brightnessCurve;
    float dotSize = threshold;

    #ifdef DITHER_QUANTIZE
        dotSize *= thresholdTweak;
    #endif

    // Contrast adjustment
    float contrast = uDitherContrast * scaleExp * brightnessSpacingMultiplier * 0.1;
    contrast *= pow(freq.y / max(freq.x, 0.001), uDitherStretchSmooth);

    // Apply contrast to create sharp dots from radial gradient pattern
    float base = threshold * (1.0 - contrast);
    float dithered = (pattern - base) * contrast;
    float result = clamp(dithered + base, 0.0, 1.0);

    // Debug: visualize fractal level
    #ifdef DITHER_DEBUG
        vec3 uvVis = vec3(frac(uvScaled.x), frac(uvScaled.y), subLayer);
        result = mix(result, uvVis.r, 0.7);
    #endif

    return vec3(result);
}

/**
 * Apply dithering to a full color (grayscale mode).
 */
vec3 dither3DGrayscale(vec2 uv, vec4 screenPos, vec3 color) {
    float brightness = getBrightness(color);
    vec3 result = applyDither3D(uv, screenPos, vec3(brightness));
    return result;
}

/**
 * Apply dithering per-channel (RGB mode).
 */
vec3 dither3DRGB(vec2 uv, vec4 screenPos, vec3 color) {
    vec3 result;
    result.r = applyDither3D(uv, screenPos, vec3(color.r)).r;
    result.g = applyDither3D(uv, screenPos, vec3(color.g)).g;
    result.b = applyDither3D(uv, screenPos, vec3(color.b)).b;
    return result;
}

/**
 * Apply dithering with CMYK halftone (print-style).
 * Each channel uses a different rotation angle to avoid moire.
 */
vec3 dither3DCMYK(vec2 uv, vec4 screenPos, vec3 color) {
    vec4 cmyk = rgbToCmyk(color);

    // CMYK angles: C=15°, M=75°, Y=0°, K=45°
    float angleC = 15.0 * 3.14159265 / 180.0;
    float angleM = 75.0 * 3.14159265 / 180.0;
    float angleY = 0.0;
    float angleK = 45.0 * 3.14159265 / 180.0;

    vec2 dirC = vec2(cos(angleC), sin(angleC));
    vec2 dirM = vec2(cos(angleM), sin(angleM));
    vec2 dirY = vec2(cos(angleY), sin(angleY));
    vec2 dirK = vec2(cos(angleK), sin(angleK));

    cmyk.r = applyDither3D(rotateUV(uv, dirC), screenPos, vec3(cmyk.r)).r;
    cmyk.g = applyDither3D(rotateUV(uv, dirM), screenPos, vec3(cmyk.g)).g;
    cmyk.b = applyDither3D(rotateUV(uv, dirY), screenPos, vec3(cmyk.b)).b;
    cmyk.a = applyDither3D(rotateUV(uv, dirK), screenPos, vec3(cmyk.a)).a;

    return cmykToRgb(cmyk);
}

/**
 * Main entry point for Dither3D integration.
 *
 * @param uv        Surface UV coordinates
 * @param screenPos Clip-space position (from vertex shader)
 * @param color     Input albedo color (RGB, already tone-mapped)
 * @return          Dithered color
 */
vec3 dither3D(vec2 uv, vec4 screenPos, vec3 color) {
    if (uDitherColorMode == 1) {
        return dither3DRGB(uv, screenPos, color);
    } else if (uDitherColorMode == 2) {
        return dither3DCMYK(uv, screenPos, color);
    }
    return dither3DGrayscale(uv, screenPos, color);
}
