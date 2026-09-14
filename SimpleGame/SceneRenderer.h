#pragma once

#include "Dependencies/glew.h"
#include "PostProcessor.h"
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <set>

struct Point2
{
    float x, y;
};

// RGB is authored in display/sRGB space. Energy > 1 is for HDR emitters only.
struct Color
{
    float r, g, b, a = 1.0f, energy = 1.0f;
};

// Pixel-space triangle batch; independent of the world and its persistence.
class SceneRenderer
{
  public:
    bool Initialize();
    void Shutdown();
    void Begin(int width, int height);
    // Flush world geometry, composite HDR to the window, then switch to sharp UI.
    void FinishWorld();

    PostProcessSettings& Effects()
    {
        return postProcessor_.Settings();
    }

    bool PostProcessingAvailable() const
    {
        return postProcessor_.Available();
    }

    void Flush();
    bool BeginMesh(const std::string& key, Point2 origin, float scale);
    void EndMesh();
    void DrawMesh(const std::string& key, Point2 origin, float scale, float opacity = 1.0f);
    bool MeshCovers(const std::string& key, Point2 origin, float scale, Point2 point) const;
    void RetainTerrainMeshes(const std::set<std::string>& keys);

    size_t MeshCount() const
    {
        return meshes_.size();
    }

    void Triangle(Point2 a, Point2 b, Point2 c, Color color);
    void Quad(Point2 a, Point2 b, Point2 c, Point2 d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Line(Point2 a, Point2 b, float width, Color color);
    void Ellipse(Point2 center, float rx, float ry, Color color);
    void Text(float x, float y, const std::string& text, Color color, float scale = 2.0f);
    float KoreanTextHeight(const std::wstring& text, int fontSize, int wrapWidth);
    void KoreanText(float x,
                    float y,
                    const std::wstring& text,
                    Color color,
                    int fontSize = 20,
                    int wrapWidth = 600);

  private:
    struct Vertex
    {
        float x, y, r, g, b, a, u = 0, v = 0, energy = 1.0f;
    };

    struct TextImage
    {
        GLuint texture = 0;
        int width = 0, height = 0;
    };

    struct Mesh
    {
        GLuint vao = 0;
        GLuint vbo = 0;
        std::vector<Vertex> vertices;
    };

    void ConfigureAttributes();
    void ReleaseMesh(Mesh& mesh);
    std::map<std::string, Mesh> meshes_;
    std::string recordingKey_;
    Point2 recordingOrigin_{};
    float recordingScale_ = 1.0f;

    using TextKey = std::tuple<std::wstring, int, int>;
    const TextImage* PrepareKoreanText(const std::wstring& text, int fontSize, int wrapWidth);
    std::map<TextKey, TextImage> textCache_;
    std::vector<Vertex> vertices_;
    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLint viewport_ = -1;
    GLint meshOffset_ = -1;
    GLint meshScale_ = -1;
    GLint meshOpacity_ = -1;
    GLint textured_ = -1;
    GLint linearOutput_ = -1;
    GLuint drawingTexture_ = 0;
    PostProcessor postProcessor_;
    bool hdrWorld_ = false;
    int width_ = 1, height_ = 1;
};
