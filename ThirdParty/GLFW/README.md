# GLFW dependency (Windows game integration)

- Version: GLFW 3.5.1, pinned release (not a moving branch).
- Official source: https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
- Source ZIP SHA-256: `EA79BC5FEFFC254C87291980C2D0BCE9ACEBB68C4983B79F961DCD2CB8A611A0`
- `vendor/`: complete source release, copied without source changes. Includes public headers,
  implementation, upstream CMake files, documentation, examples, tests, dependency headers and licenses.
- `vendor/LICENSE.md`: zlib/libpng license. Keep this with redistributed source/binaries.
- `glfw.vcxproj`: project-owned MSBuild integration, not an upstream generated file.

Open the root `SimpleGame.sln`. Its project reference builds `glfw3.lib` from the vendored C source
with the same Win32/x64 and Debug/Release configuration as the game. It uses MSVC v143 and the
Windows 10 SDK, matching the existing game project. GLFW itself uses C11; the game uses C++17.

No CMake executable, vcpkg, NuGet, network download, prebuilt GLFW library or GLFW DLL is needed
for a normal Visual Studio solution build. Generated static libraries/objects stay in `build/`
and are intentionally not committed. Both the public headers and all 21 Win32/common C units
listed by upstream `src/CMakeLists.txt` are included in the repository.

Do not define `GLFW_DLL` in the application, or `GLFW_INCLUDE_NONE` in the GLFW library project.
The application defines `GLFW_INCLUDE_NONE` to keep its GLEW OpenGL headers in control.
`_GLFW_WIN32` is defined only while compiling the GLFW implementation.

The full upstream source is retained for future platforms. The **game** is still Windows-specific
(Win32 dialogs, fonts, monitoring and filesystem APIs); this migration does not claim Linux/macOS
game build support. See `Doc/GLFW_Migration.md` for runtime and profiling details.
