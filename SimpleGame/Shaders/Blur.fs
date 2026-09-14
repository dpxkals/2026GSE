#version 330 core
in vec2 uv;
out vec4 fragment;
uniform sampler2D sourceImage;
uniform vec2 sampleStep;

void main()
{
    vec3 color = texture(sourceImage, uv).rgb * 0.2270270270;
    color +=
        (texture(sourceImage, uv + sampleStep).rgb + texture(sourceImage, uv - sampleStep).rgb) *
        0.1945945946;
    color += (texture(sourceImage, uv + sampleStep * 2.0).rgb +
              texture(sourceImage, uv - sampleStep * 2.0).rgb) *
             0.1216216216;
    color += (texture(sourceImage, uv + sampleStep * 3.0).rgb +
              texture(sourceImage, uv - sampleStep * 3.0).rgb) *
             0.0540540541;
    color += (texture(sourceImage, uv + sampleStep * 4.0).rgb +
              texture(sourceImage, uv - sampleStep * 4.0).rgb) *
             0.0162162162;
    fragment = vec4(color, 1.0);
}
