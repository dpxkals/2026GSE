#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in float energy;
layout(location = 4) in vec4 instanceTransform;
layout(location = 5) in vec4 instanceColor;
layout(location = 6) in float instanceEnergy;
layout(location = 7) in vec2 instanceShear;

uniform vec2 viewport;
uniform vec2 meshOffset;
uniform float meshScale;
uniform float meshOpacity;
uniform bool instanced;

out vec2 uv;
out float radiance;
out vec4 tint;

void main()
{
    vec2 pixel = position * meshScale + meshOffset;
    if (instanced)
    {
        pixel = position * instanceTransform.zw + position.yx * instanceShear + instanceTransform.xy;
    }
    gl_Position =
        vec4(pixel.x / viewport.x * 2.0 - 1.0, 1.0 - pixel.y / viewport.y * 2.0, 0.0, 1.0);

    tint = vec4(color.rgb, color.a * meshOpacity);
    uv = texcoord;
    radiance = energy;
    if (instanced)
    {
        tint = color * instanceColor;
        radiance = energy * instanceEnergy;
    }
}
