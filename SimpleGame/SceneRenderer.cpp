#include "stdafx.h"
#include "FrameProfiler.h"
#include "SceneRenderer.h"
#include "ShaderProgram.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <Windows.h>

namespace
{
    // Five column, seven row ASCII glyphs, least significant bit at the top.
    const unsigned char* Glyph(char c)
    {
        static const unsigned char letters[][5] = {
            {126, 17, 17, 17, 126}, {127, 73, 73, 73, 54}, {62, 65, 65, 65, 34},
            {127, 65, 65, 34, 28},  {127, 73, 73, 73, 65}, {127, 9, 9, 9, 1},
            {62, 65, 73, 73, 122},  {127, 8, 8, 8, 127},   {0, 65, 127, 65, 0},
            {32, 64, 65, 63, 1},    {127, 8, 20, 34, 65},  {127, 64, 64, 64, 64},
            {127, 2, 12, 2, 127},   {127, 4, 8, 16, 127},  {62, 65, 65, 65, 62},
            {127, 9, 9, 9, 6},      {62, 65, 81, 33, 94},  {127, 9, 25, 41, 70},
            {70, 73, 73, 73, 49},   {1, 1, 127, 1, 1},     {63, 64, 64, 64, 63},
            {31, 32, 64, 32, 31},   {63, 64, 56, 64, 63},  {99, 20, 8, 20, 99},
            {3, 4, 120, 4, 3},      {97, 81, 73, 69, 67}};
        static const unsigned char digits[][5] = {{62, 81, 73, 69, 62},
                                                  {0, 66, 127, 64, 0},
                                                  {66, 97, 81, 73, 70},
                                                  {33, 65, 69, 75, 49},
                                                  {24, 20, 18, 127, 16},
                                                  {39, 69, 69, 69, 57},
                                                  {60, 74, 73, 73, 48},
                                                  {1, 113, 9, 5, 3},
                                                  {54, 73, 73, 73, 54},
                                                  {6, 73, 73, 41, 30}};
        static const unsigned char space[] = {0, 0, 0, 0, 0};
        static const unsigned char dash[] = {8, 8, 8, 8, 8};
        static const unsigned char dot[] = {0, 96, 96, 0, 0};
        static const unsigned char colon[] = {0, 54, 54, 0, 0};
        static const unsigned char slash[] = {32, 16, 8, 4, 2};
        static const unsigned char plus[] = {8, 8, 62, 8, 8};
        static const unsigned char leftBracket[] = {0, 127, 65, 65, 0};
        static const unsigned char rightBracket[] = {0, 65, 65, 127, 0};
        if (c >= 'a' && c <= 'z')
        {
            c -= 'a' - 'A';
        }
        if (c >= 'A' && c <= 'Z')
        {
            return letters[c - 'A'];
        }
        if (c >= '0' && c <= '9')
        {
            return digits[c - '0'];
        }
        switch (c)
        {
            case '-':
                return dash;
            case '.':
                return dot;
            case ':':
                return colon;
            case '/':
                return slash;
            case '+':
                return plus;
            case '[':
                return leftBracket;
            case ']':
                return rightBracket;
        }
        return space;
    }
}

bool SceneRenderer::Initialize()
{
    program_ = ShaderProgram::Load(L"Scene.vs", L"Scene.fs");
    if (!program_)
    {
        return false;
    }
    viewport_ = glGetUniformLocation(program_, "viewport");
    meshOffset_ = glGetUniformLocation(program_, "meshOffset");
    meshScale_ = glGetUniformLocation(program_, "meshScale");
    meshOpacity_ = glGetUniformLocation(program_, "meshOpacity");
    textured_ = glGetUniformLocation(program_, "textured");
    linearOutput_ = glGetUniformLocation(program_, "linearOutput");
    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "textImage"), 0);
    glUseProgram(0);
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    ConfigureAttributes();
    glBindVertexArray(0);
    vertices_.reserve(150000);
    bool initialized = vao_ && vbo_ && viewport_ >= 0 && linearOutput_ >= 0 && meshOffset_ >= 0 &&
                       meshScale_ >= 0 && meshOpacity_ >= 0;
    if (initialized)
    {
        postProcessor_.Initialize(); // failure falls back to the direct path
    }
    return initialized;
}

void SceneRenderer::Shutdown()
{
    postProcessor_.Shutdown();
    for (auto& entry : meshes_)
    {
        ReleaseMesh(entry.second);
    }
    meshes_.clear();
    for (const auto& entry : textCache_)
    {
        glDeleteTextures(1, &entry.second.texture);
    }
    textCache_.clear();
    drawingTexture_ = 0;
    if (vbo_)
    {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_)
    {
        glDeleteVertexArrays(1, &vao_);
    }
    if (program_)
    {
        glDeleteProgram(program_);
    }
    vbo_ = vao_ = program_ = 0;
}

void SceneRenderer::Begin(int width, int height)
{
    width_ = (std::max)(width, 1);
    height_ = (std::max)(height, 1);
    vertices_.clear();
    hdrWorld_ = postProcessor_.Begin(width_, height_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    auto background = [this](float value)
    {
        return hdrWorld_ ? std::pow((value + 0.055f) / 1.055f, 2.4f) : value;
    };
    glClearColor(background(0.045f), background(0.065f), background(0.075f), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void SceneRenderer::FinishWorld()
{
    Flush();
    postProcessor_.Composite();
    hdrWorld_ = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
}

void SceneRenderer::Flush()
{
    if (!recordingKey_.empty() || vertices_.empty())
    {
        return;
    }
    glUseProgram(program_);
    glUniform2f(viewport_, float(width_), float(height_));
    glUniform2f(meshOffset_, 0.0f, 0.0f);
    glUniform1f(meshScale_, 1.0f);
    glUniform1f(meshOpacity_, 1.0f);
    glUniform1i(textured_, drawingTexture_ != 0);
    glUniform1i(linearOutput_, hdrWorld_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawingTexture_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER, vertices_.size() * sizeof(Vertex), vertices_.data(), GL_STREAM_DRAW);
    FrameProfiler::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    drawingTexture_ = 0;
    vertices_.clear();
}

void SceneRenderer::Triangle(Point2 a, Point2 b, Point2 c, Color color)
{
    for (Point2 p : {a, b, c})
    {
        vertices_.push_back({p.x, p.y, color.r, color.g, color.b, color.a, 0, 0, color.energy});
    }
}

void SceneRenderer::Quad(Point2 a, Point2 b, Point2 c, Point2 d, Color color)
{
    Triangle(a, b, c, color);
    Triangle(a, c, d, color);
}

void SceneRenderer::Rect(float x, float y, float w, float h, Color color)
{
    Quad({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, color);
}

void SceneRenderer::Line(Point2 a, Point2 b, float width, Color color)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f)
    {
        return;
    }
    float ox = -dy / length * width * 0.5f, oy = dx / length * width * 0.5f;
    Quad({a.x + ox, a.y + oy},
         {b.x + ox, b.y + oy},
         {b.x - ox, b.y - oy},
         {a.x - ox, a.y - oy},
         color);
}

void SceneRenderer::Ellipse(Point2 p, float rx, float ry, Color color)
{
    const int segments = 20;
    for (int i = 0; i < segments; ++i)
    {
        float a = i * 6.2831853f / segments, b = (i + 1) * 6.2831853f / segments;
        Triangle(p,
                 {p.x + std::cos(a) * rx, p.y + std::sin(a) * ry},
                 {p.x + std::cos(b) * rx, p.y + std::sin(b) * ry},
                 color);
    }
}

void SceneRenderer::Text(float x, float y, const std::string& text, Color color, float scale)
{
    float start = x;
    for (char c : text)
    {
        if (c == '\n')
        {
            x = start;
            y += 10 * scale;
            continue;
        }
        const unsigned char* glyph = Glyph(c);
        for (int col = 0; col < 5; ++col)
        {
            for (int row = 0; row < 7; ++row)
            {
                if (glyph[col] & (1 << row))
                {
                    Rect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

const SceneRenderer::TextImage* SceneRenderer::PrepareKoreanText(const std::wstring& text,
                                                                 int fontSize,
                                                                 int wrapWidth)
{
    if (text.empty())
    {
        return nullptr;
    }
    fontSize = (std::max)(12, (std::min)(fontSize, 48));
    wrapWidth = (std::max)(80, (std::min)(wrapWidth, 2048));
    TextKey key{text, fontSize, wrapWidth};
    auto cached = textCache_.find(key);
    if (cached != textCache_.end())
    {
        return &cached->second;
    }

    // Win32 rasterization is used only on cache misses. OpenGL draws the cached mask.
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc)
    {
        return nullptr;
    }
    HFONT font = CreateFontW(-fontSize,
                             0,
                             0,
                             0,
                             FW_NORMAL,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             L"Malgun Gothic");
    if (!font)
    {
        DeleteDC(dc);
        return nullptr;
    }
    HGDIOBJ oldFont = SelectObject(dc, font);
    RECT bounds{0, 0, wrapWidth, 0};
    constexpr UINT flags = DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX;
    int measured =
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &bounds, flags | DT_CALCRECT);
    int bitmapWidth = (std::max)(wrapWidth, int(bounds.right));
    int bitmapHeight = int(bounds.bottom) + 4;
    if (measured <= 0 || bitmapWidth > 4096 || bitmapHeight > 2048)
    {
        SelectObject(dc, oldFont);
        DeleteObject(font);
        DeleteDC(dc);
        return nullptr;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = bitmapWidth;
    info.bmiHeader.biHeight = -bitmapHeight; // top-down, matching the UI coordinates
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap)
    {
        SelectObject(dc, oldFont);
        DeleteObject(font);
        DeleteDC(dc);
        return nullptr;
    }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    PatBlt(dc, 0, 0, bitmapWidth, bitmapHeight, BLACKNESS);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT destination{0, 0, wrapWidth, bitmapHeight};
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &destination, flags);
    GdiFlush();
    std::vector<unsigned char> mask(static_cast<size_t>(bitmapWidth) * bitmapHeight);
    const auto* source = static_cast<const unsigned char*>(pixels);
    for (size_t i = 0; i < mask.size(); ++i)
    {
        mask[i] = (std::max)(source[i * 4], (std::max)(source[i * 4 + 1], source[i * 4 + 2]));
    }
    SelectObject(dc, oldBitmap);
    SelectObject(dc, oldFont);
    DeleteObject(bitmap);
    DeleteObject(font);
    DeleteDC(dc);

    Flush(); // preserve painter order before touching textures
    if (textCache_.size() >= 96)
    {
        for (const auto& entry : textCache_)
        {
            glDeleteTextures(1, &entry.second.texture);
        }
        textCache_.clear();
    }
    TextImage image{};
    image.width = bitmapWidth;
    image.height = bitmapHeight;
    glGenTextures(1, &image.texture);
    if (!image.texture)
    {
        return nullptr;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image.texture);
    GLint alignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_R8,
                 bitmapWidth,
                 bitmapHeight,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 mask.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return &textCache_.emplace(std::move(key), image).first->second;
}

float SceneRenderer::KoreanTextHeight(const std::wstring& text, int fontSize, int wrapWidth)
{
    const TextImage* image = PrepareKoreanText(text, fontSize, wrapWidth);
    return image ? float(image->height) : float(fontSize + 4);
}

void SceneRenderer::KoreanText(
    float x, float y, const std::wstring& text, Color color, int fontSize, int wrapWidth)
{
    const TextImage* image = PrepareKoreanText(text, fontSize, wrapWidth);
    if (!image)
    {
        Text(x, y, "KOREAN FONT ERROR", color, 1.5f);
        return;
    }
    Flush();
    drawingTexture_ = image->texture;
    float right = x + image->width, bottom = y + image->height;
    for (auto corner : {std::pair<Point2, Point2>{{x, y}, {0, 0}},
                        {{right, y}, {1, 0}},
                        {{right, bottom}, {1, 1}},
                        {{x, y}, {0, 0}},
                        {{right, bottom}, {1, 1}},
                        {{x, bottom}, {0, 1}}})
    {
        vertices_.push_back({corner.first.x,
                             corner.first.y,
                             color.r,
                             color.g,
                             color.b,
                             color.a,
                             corner.second.x,
                             corner.second.y});
    }
    Flush();
}

void SceneRenderer::ConfigureAttributes()
{
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, energy)));
}
