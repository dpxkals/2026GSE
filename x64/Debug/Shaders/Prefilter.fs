#version 330 core
in vec2 uv;
out vec4 fragment;
uniform sampler2D sourceImage;
uniform vec2 sourceTexel;
uniform bool extractHighlights;
uniform float threshold;

vec3 readSample(vec2 p)
{
    vec3 color = max(texture(sourceImage, p).rgb, vec3(0.0));

    if (!extractHighlights)
    {
        return color;
    }

    // Soft knee avoids a hard contour as a small flame crosses the threshold.
    float brightness = max(color.r, max(color.g, color.b));
    float knee = max(threshold * 0.5, 0.0001);
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(soft, brightness - threshold) / max(brightness, 0.0001);

    return color * contribution;
}

void main()
{
    vec2 d = sourceTexel * 0.5;
    // Extract BEFORE averaging so a small emissive pixel isn't lost on downsampling.
    vec3 color = readSample(uv + vec2(-d.x, -d.y));
    color += readSample(uv + vec2(d.x, -d.y));
    color += readSample(uv + vec2(-d.x, d.y));
    color += readSample(uv + vec2(d.x, d.y));
    fragment = vec4(color * 0.25, 1.0);
}
