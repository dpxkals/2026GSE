# GLEW runtime files

These are copies of the existing repository's DLLs, not a GLEW upgrade.
The headers/import libraries remain in `Dependencies/` (Win32) and `Dependencies/x64/`.
MSBuild selects `bin/$(Platform)/glew32.dll` and copies it next to the game executable
for both Debug and Release, including custom output directories.

| File | PE machine | SHA-256 | Original tracked file |
| --- | --- | --- | --- |
| Win32/glew32.dll | 014C (x86) | 8C7805A05A321FAB37A31822EA2D2A6997DDFD130CF0A09A54A428D6E6995700 | Debug/glew32.dll |
| x64/glew32.dll | 8664 (x64) | EABB3DFEC3A95F0282F10F5BA4B633E681AA5AFF19995CAD069E6E954A9CEA12 | x64/Debug/glew32.dll |

Copyright notices are reproduced in `../GLEW_LICENSES.md` from the bundled `glew.h`.
GLFW is linked statically; FreeGLUT DLLs are no longer required.
