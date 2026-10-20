# Asset loader dependencies

These files are vendored without formatting or source modifications.

| File | Upstream version / source | License | SHA-256 |
| --- | --- | --- | --- |
| cgltf.h | [cgltf v1.15](https://github.com/jkuhlmann/cgltf/blob/v1.15/cgltf.h) | MIT, notice embedded at end of header | E378A21C084BF1F288BB799DE827BB26906EFB024255F1ECF1705EA13F11C6EC |
| json.hpp | [nlohmann/json v3.11.3](https://github.com/nlohmann/json/blob/v3.11.3/single_include/nlohmann/json.hpp) | MIT, notice embedded in header | 9BEA4C8066EF4A1C206B2BE5A36302F8926F7FDC6087AF5D20B417D0CF103EA6 |

`CGLTF_IMPLEMENTATION` is defined once, in `GltfLoader.cpp`.
No network access, Python, or Blender installation is required at game runtime.

Format reference: [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

## MIT notices

cgltf: Copyright (c) 2018-2021 Johannes Kuhlmann

JSON for Modern C++: Copyright (c) 2013-2023 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
