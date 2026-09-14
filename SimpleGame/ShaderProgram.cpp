#include "stdafx.h"
#include "ShaderProgram.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    GLuint Compile(GLenum type, const std::filesystem::path& file)
    {
        std::ifstream input(file, std::ios::binary);
        if (!input)
        {
            std::wcerr << L"Cannot open shader: " << file.c_str() << L'\n';
            return 0;
        }
        std::string source((std::istreambuf_iterator<char>(input)), {});
        if (source.size() >= 3 && source.compare(0, 3, "\xEF\xBB\xBF") == 0)
        {
            source.erase(0, 3);
        }
        GLuint shader = glCreateShader(type);
        if (!shader)
        {
            return 0;
        }
        const char* data = source.c_str();
        glShaderSource(shader, 1, &data, nullptr);
        glCompileShader(shader);
        GLint success = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[4096] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::wcerr << L"Shader compile failed: " << file.c_str() << L'\n';
            std::cerr << log << '\n';
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
}

GLuint ShaderProgram::Load(const wchar_t* vertexFile, const wchar_t* fragmentFile)
{
    wchar_t executable[32768] = {};
    DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        std::cerr << "Cannot resolve shader directory.\n";
        return 0;
    }
    auto directory = std::filesystem::path(executable).parent_path() / L"Shaders";
    GLuint vertex = Compile(GL_VERTEX_SHADER, directory / vertexFile);
    GLuint fragment = Compile(GL_FRAGMENT_SHADER, directory / fragmentFile);
    GLuint program = 0;
    if (vertex && fragment)
    {
        program = glCreateProgram();
        if (program)
        {
            glAttachShader(program, vertex);
            glAttachShader(program, fragment);
            glLinkProgram(program);
            GLint success = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (!success)
            {
                char log[4096] = {};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                std::wcerr << L"Shader link failed: " << vertexFile << L" / " << fragmentFile
                           << L'\n';
                std::cerr << log << '\n';
                glDeleteProgram(program);
                program = 0;
            }
        }
    }
    if (vertex)
    {
        glDeleteShader(vertex);
    }
    if (fragment)
    {
        glDeleteShader(fragment);
    }
    return program;
}
