#version 330 core
in vec2 uv;
out vec4 fragment;
uniform sampler2D sceneImage;
uniform sampler2D bloomImage;
uniform sampler2D blurredImage;
uniform float bloomStrength;
uniform float blurStrength;
uniform float vignetteStrength;
uniform float exposure;

vec3 toneMap(vec3 x)
{
    // ACES-style filmic fit, not a complete ACES color-management pipeline.
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 linearToSRGB(vec3 c)
{
    vec3 low = 12.92 * c;
    vec3 high = 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;

    return mix(high, low, lessThanEqual(c, vec3(0.0031308)));
}

void main()
{
    // Ellipse normalized to the viewport; focus follows the camera's player anchor.
    // UV origin is bottom-left, whereas Project() uses top-left pixel coordinates.
    float radius = length((uv - vec2(0.5, 0.48)) / vec2(0.5));
    float blurMask = smoothstep(0.45, 1.20, radius) * blurStrength;
    vec3 color = texture(sceneImage, uv).rgb;

    if (blurStrength > 0.0)
    {
        color = mix(color, texture(blurredImage, uv).rgb, blurMask);
    }

    if (bloomStrength > 0.0)
    {
        color += texture(bloomImage, uv).rgb * bloomStrength;
    }

    color *= 1.0 - vignetteStrength * smoothstep(0.40, 1.30, radius);
    color = toneMap(max(color, vec3(0.0)) * exposure);
    // Explicit encoding, GL_FRAMEBUFFER_SRGB is kept disabled to avoid double gamma.
    fragment = vec4(linearToSRGB(color), 1.0);
}
