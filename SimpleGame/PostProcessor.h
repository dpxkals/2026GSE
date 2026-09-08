#pragma once

#include "Dependencies/glew.h"

struct PostProcessSettings
{
    bool enabled = true;
    bool bloom = true;
    bool vignette = true;
    bool edgeBlur = true;
    float exposure = 1.1f;
    float bloomStrength = 0.35f;
    float bloomThreshold = 1.0f;
    float vignetteStrength = 0.48f;
    float edgeBlurStrength = 0.85f;
};

// Owns HDR render targets and full-screen passes, not gameplay or UI.
// Initialize/Shutdown must run while the GL context is current.
class PostProcessor
{
public:
    PostProcessor() = default;
    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;
    bool Initialize();
    void Shutdown();
    bool Begin(int width, int height);
    void Composite();
    PostProcessSettings& Settings() { return settings_; }
    const PostProcessSettings& Settings() const { return settings_; }
    bool Available() const { return ready_ && (width_ == 0 || targetsReady_); }
private:
    struct Target { GLuint framebuffer = 0, texture = 0; };
    struct PrefilterUniforms { GLint source, texel, extract, threshold; } prefilter_{};
    struct BlurUniforms { GLint source, step; } blur_{};
    struct CompositeUniforms
    {
        GLint scene, bloomImage, blurredImage, bloomStrength, blurStrength;
        GLint vignetteStrength, exposure;
    } composite_{};
    bool ResizeTargets(int width, int height);
    static bool CreateTarget(Target& target, int width, int height);
    void ReleaseTargets();
    void Filter(GLuint source, Target (&targets)[2], bool extract, int passes, float radius);
    void Draw(GLuint framebuffer, int width, int height, GLuint program);
    PostProcessSettings settings_;
    GLuint vao_ = 0, prefilterProgram_ = 0, blurProgram_ = 0, compositeProgram_ = 0;
    Target scene_, bloom_[2], blurred_[2];
    int width_ = 0, height_ = 0, halfWidth_ = 0, halfHeight_ = 0;
    bool ready_ = false, targetsReady_ = false, active_ = false;
};
