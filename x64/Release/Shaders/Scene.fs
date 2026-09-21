#version 330 core
in vec4 tint;
in vec2 uv;
in float radiance;

uniform sampler2D textImage;
uniform bool textured;
uniform bool linearOutput;

out vec4 fragment;

vec3 decode(vec3 c)
{
    return mix(pow(max((c + 0.055) / 1.055, vec3(0.0)), vec3(2.4)),
               c / 12.92,
               lessThanEqual(c, vec3(0.04045)));
}

void main()
{
    vec3 rgb = linearOutput ? decode(tint.rgb) * radiance : tint.rgb;

    fragment = vec4(rgb, tint.a * (textured ? texture(textImage, uv).r : 1.0));
}
