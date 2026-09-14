#pragma once

#include "Dependencies/glew.h"

namespace ShaderProgram
{
    // All runtime shader paths are relative to the executable, never the working directory.
    GLuint Load(const wchar_t* vertexFile, const wchar_t* fragmentFile);
}
