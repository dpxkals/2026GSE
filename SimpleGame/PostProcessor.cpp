#include "stdafx.h"
#include "PostProcessor.h"
#include <algorithm>
#include <iostream>

namespace
{
    const char* FullscreenVertex = R"GLSL(#version 330 core
out vec2 uv;
void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

    const char* PrefilterFragment = R"GLSL(#version 330 core
in vec2 uv;
out vec4 fragment;
uniform sampler2D sourceImage;
uniform vec2 sourceTexel;
uniform bool extractHighlights;
uniform float threshold;
vec3 readSample(vec2 p)
{
    vec3 color = max(texture(sourceImage, p).rgb, vec3(0.0));
    if (!extractHighlights) return color;
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
)GLSL";

    const char* BlurFragment = R"GLSL(#version 330 core
in vec2 uv;
out vec4 fragment;
uniform sampler2D sourceImage;
uniform vec2 sampleStep;
void main()
{
    vec3 color = texture(sourceImage, uv).rgb * 0.2270270270;
    color += (texture(sourceImage, uv + sampleStep).rgb +
              texture(sourceImage, uv - sampleStep).rgb) * 0.1945945946;
    color += (texture(sourceImage, uv + sampleStep * 2.0).rgb +
              texture(sourceImage, uv - sampleStep * 2.0).rgb) * 0.1216216216;
    color += (texture(sourceImage, uv + sampleStep * 3.0).rgb +
              texture(sourceImage, uv - sampleStep * 3.0).rgb) * 0.0540540541;
    color += (texture(sourceImage, uv + sampleStep * 4.0).rgb +
              texture(sourceImage, uv - sampleStep * 4.0).rgb) * 0.0162162162;
    fragment = vec4(color, 1.0);
}
)GLSL";

    const char* CompositeFragment = R"GLSL(#version 330 core
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
        color = mix(color, texture(blurredImage, uv).rgb, blurMask);
    if (bloomStrength > 0.0)
        color += texture(bloomImage, uv).rgb * bloomStrength;
    color *= 1.0 - vignetteStrength * smoothstep(0.40, 1.30, radius);
    color = toneMap(max(color, vec3(0.0)) * exposure);
    // Explicit encoding, GL_FRAMEBUFFER_SRGB is kept disabled to avoid double gamma.
    fragment = vec4(linearToSRGB(color), 1.0);
}
)GLSL";

    GLuint Shader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        if (!shader) return 0;
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[2048] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Post-processing shader: " << log << '\n';
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint Program(const char* fragmentSource)
    {
        GLuint vertex = Shader(GL_VERTEX_SHADER, FullscreenVertex);
        GLuint fragment = Shader(GL_FRAGMENT_SHADER, fragmentSource);
        GLuint program = 0;
        if (vertex && fragment)
        {
            program = glCreateProgram();
            if (program)
            {
                glAttachShader(program, vertex);
                glAttachShader(program, fragment);
                glLinkProgram(program);
                GLint ok = GL_FALSE;
                glGetProgramiv(program, GL_LINK_STATUS, &ok);
                if (!ok)
                {
                    char log[2048] = {};
                    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                    std::cerr << "Post-processing program: " << log << '\n';
                    glDeleteProgram(program); program = 0;
                }
            }
        }
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return program;
    }
}

bool PostProcessor::Initialize()
{
    prefilterProgram_ = Program(PrefilterFragment);
    blurProgram_ = Program(BlurFragment);
    compositeProgram_ = Program(CompositeFragment);
    glGenVertexArrays(1, &vao_);
    if (!prefilterProgram_ || !blurProgram_ || !compositeProgram_ || !vao_)
    {
        Shutdown();
        std::cerr << "Post-processing unavailable; using direct rendering.\n";
        return false;
    }
    prefilter_ = {glGetUniformLocation(prefilterProgram_,"sourceImage"),
        glGetUniformLocation(prefilterProgram_,"sourceTexel"),
        glGetUniformLocation(prefilterProgram_,"extractHighlights"),
        glGetUniformLocation(prefilterProgram_,"threshold")};
    blur_ = {glGetUniformLocation(blurProgram_,"sourceImage"),
        glGetUniformLocation(blurProgram_,"sampleStep")};
    composite_ = {glGetUniformLocation(compositeProgram_,"sceneImage"),
        glGetUniformLocation(compositeProgram_,"bloomImage"),
        glGetUniformLocation(compositeProgram_,"blurredImage"),
        glGetUniformLocation(compositeProgram_,"bloomStrength"),
        glGetUniformLocation(compositeProgram_,"blurStrength"),
        glGetUniformLocation(compositeProgram_,"vignetteStrength"),
        glGetUniformLocation(compositeProgram_,"exposure")};
    ready_ = true;
    return true;
}

bool PostProcessor::CreateTarget(Target& target, int width, int height)
{
    glGenTextures(1, &target.texture);
    glGenFramebuffers(1, &target.framebuffer);
    if (!target.texture || !target.framebuffer) return false;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void PostProcessor::ReleaseTargets()
{
    for (Target* target : {&scene_, &bloom_[0], &bloom_[1], &blurred_[0], &blurred_[1]})
    {
        if (target->framebuffer) glDeleteFramebuffers(1, &target->framebuffer);
        if (target->texture) glDeleteTextures(1, &target->texture);
        *target = {};
    }
    targetsReady_ = false;
}

void PostProcessor::Shutdown()
{
    ReleaseTargets();
    if (prefilterProgram_) glDeleteProgram(prefilterProgram_);
    if (blurProgram_) glDeleteProgram(blurProgram_);
    if (compositeProgram_) glDeleteProgram(compositeProgram_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = prefilterProgram_ = blurProgram_ = compositeProgram_ = 0;
    ready_ = active_ = false;
    width_ = height_ = 0;
}

bool PostProcessor::ResizeTargets(int width, int height)
{
    if (width_ == width && height_ == height) return targetsReady_;
    ReleaseTargets();
    width_ = width; height_ = height;
    halfWidth_ = (std::max)(1, (width + 1) / 2);
    halfHeight_ = (std::max)(1, (height + 1) / 2);
    GLint maxTexture = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    bool ok = width <= maxTexture && height <= maxTexture;
    if (ok) ok = CreateTarget(scene_, width, height);
    for (Target* target : {&bloom_[0], &bloom_[1], &blurred_[0], &blurred_[1]})
        if (ok) ok = CreateTarget(*target, halfWidth_, halfHeight_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!ok)
    {
        ReleaseTargets();
        // Do not retry every frame at the same failing size.
        std::cerr << "HDR framebuffer allocation failed at " << width << 'x' << height
            << "; direct rendering until the next resize.\n";
        return false;
    }
    targetsReady_ = true;
    return true;
}

bool PostProcessor::Begin(int width, int height)
{
    active_ = settings_.enabled && ready_ && ResizeTargets(width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, active_ ? scene_.framebuffer : 0);
    return active_;
}

void PostProcessor::Draw(GLuint framebuffer, int width, int height, GLuint program)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glUseProgram(program);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void PostProcessor::Filter(GLuint source, Target (&targets)[2], bool extract, int passes, float radius)
{
    glUseProgram(prefilterProgram_);
    glUniform1i(prefilter_.source, 0);
    glUniform2f(prefilter_.texel, 1.0f/width_, 1.0f/height_);
    glUniform1i(prefilter_.extract, extract);
    glUniform1f(prefilter_.threshold, (std::max)(0.01f,settings_.bloomThreshold));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    Draw(targets[0].framebuffer, halfWidth_, halfHeight_, prefilterProgram_);
    glUseProgram(blurProgram_);
    glUniform1i(blur_.source, 0);
    for (int pass=0; pass<passes; ++pass)
    {
        int read = pass % 2, write = 1-read;
        glBindTexture(GL_TEXTURE_2D, targets[read].texture);
        glUniform2f(blur_.step, pass%2 == 0 ? radius/halfWidth_ : 0.0f,
            pass%2 != 0 ? radius/halfHeight_ : 0.0f);
        Draw(targets[write].framebuffer, halfWidth_, halfHeight_, blurProgram_);
    }
    // Callers use even pass counts, so the final image is always in targets[0].
}

void PostProcessor::Composite()
{
    if (!active_) return;
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    if (settings_.bloom) Filter(scene_.texture, bloom_, true, 6, 1.5f);
    if (settings_.edgeBlur) Filter(scene_.texture, blurred_, false, 4, 2.0f);
    glUseProgram(compositeProgram_);
    glUniform1i(composite_.scene, 0);
    glUniform1i(composite_.bloomImage, 1);
    glUniform1i(composite_.blurredImage, 2);
    glUniform1f(composite_.bloomStrength, settings_.bloom ? (std::max)(0.0f,settings_.bloomStrength) : 0.0f);
    glUniform1f(composite_.blurStrength, settings_.edgeBlur ? std::clamp(settings_.edgeBlurStrength,0.0f,1.0f) : 0.0f);
    glUniform1f(composite_.vignetteStrength, settings_.vignette ? std::clamp(settings_.vignetteStrength,0.0f,1.0f) : 0.0f);
    glUniform1f(composite_.exposure, std::clamp(settings_.exposure,0.1f,4.0f));
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene_.texture);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, bloom_[0].texture);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, blurred_[0].texture);
    Draw(0, width_, height_, compositeProgram_);
    for (int unit=2; unit>=0; --unit)
    {
        glActiveTexture(GL_TEXTURE0+unit);
        glBindTexture(GL_TEXTURE_2D,0);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    active_ = false;
}
