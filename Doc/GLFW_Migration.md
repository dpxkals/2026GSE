# FreeGLUT → GLFW 전환 및 성능 점검

작성: 2026-11-03. 전환 전 기준 커밋: `4fa0313` (`Apply modeling`).

## 변경 범위와 확인 수준

창/컨텍스트 생성, 이벤트, 키/마우스/휠, 프레임 스케줄링, 종료를 GLFW 3.5.1로 교체했다.
렌더러의 draw/배치/인스턴싱, 메시 캐시, 스키닝, 셰이더, HDR 후처리와 게임 규칙은 변경하지 않았다.
FreeGLUT의 활성 소스 참조와 링크 의존성은 제거했다. 기존 FreeGLUT 헤더/lib/DLL은 과거 자료로
남겨 두었지만 새 실행 파일에는 필요하지 않다.

**사용자 지침에 따라 빌드·게임 실행·테스트·FPS 측정은 수행하지 않았다.**
아래의 완료 항목은 소스/설정/파일 구조 확인이며 성능 무저하나 빌드 성공을 실측한 결과가 아니다.

## 다른 Windows PC에서 빌드

1. 저장소 전체를 clone한다. `ThirdParty/GLFW`를 제외하거나 별도로 다운받을 필요가 없다.
2. Visual Studio 2022 이상에 C++ 데스크톱 개발, MSVC **v143**, Windows 10/11 SDK를 설치한다.
   기존 게임과 동일한 툴셋을 사용하며 SDK 버전은 `10.0`으로 설치된 버전을 선택한다.
3. 루트 `SimpleGame.sln`을 연다. Debug/Release × x64/x86 네 구성을 제공한다.
4. 솔루션을 빌드한다. GLFW 프로젝트가 선택한 구성으로 `glfw3.lib`를 먼저 만들고 게임에 링크한다.
   `.sln`의 x86은 각 프로젝트의 Win32로 연결된다. 게임 vcxproj의 ProjectReference도 포함했다.
5. 실행 파일 옆에는 해당 아키텍처의 `glew32.dll`, `Shaders`, `Monitor`, `assets`, `Config`,
   `Licenses`가 자동 복사된다. 사용자 지정 출력 폴더도 `$(TargetDir)` 기준으로 처리한다.

게임은 OpenGL 3.3 드라이버와 선택한 MSVC 런타임이 필요하다. 특히 Debug 실행 파일은
개발 도구가 없는 PC에 배포할 대상이 아니다. 배포는 Release와 해당 VC++ 재배포 패키지를 사용한다.
Windows API 기반 게임을 Linux/macOS까지 이식한 변경은 아니다.

### 의존성 배치

- `ThirdParty/GLFW/vendor`: 공식 3.5.1 전체 소스 릴리스. 소스 수정 없음.
- `ThirdParty/GLFW/glfw.vcxproj`: Win32/common C 소스 21개를 정적 빌드한다.
  upstream `src/CMakeLists.txt`의 Windows 소스 목록에 맞췄다. C11, Debug `/MDd`, Release `/MD`.
- `ThirdParty/GLFW/README.md`: 다운로드 주소, 고정 버전, ZIP SHA-256, 사용 조건.
- GLFW에는 DLL/기성 ABI 바이너리를 사용하지 않는다. 다른 PC도 같은 소스를 해당 구성으로 빌드한다.
  CMake 실행 파일, vcpkg/NuGet, 네트워크, Git submodule 초기화는 필요 없다.
- 기존 GLEW 헤더/import lib는 `SimpleGame/Dependencies`, `Dependencies/x64`를 사용한다.
  링크 경로는 현재 작업 폴더에 의존하지 않도록 `$(ProjectDir)` 기준으로 바꿨다.
- GLEW runtime은 `Dependencies/bin/Win32`, `bin/x64`에 저장소 기존 DLL의 사본을 넣었다.
  PE machine과 원본 해시는 `Dependencies/bin/README.md`에 기록했다. GLEW 버전 변경은 없다.
- GLFW/GLEW 라이선스는 EXE 옆 `Licenses`에 복사한다.
- GLFW build/obj/lib는 생성물로 ignore한다. 빌드에 필요한 원본 전체와 기존 GLEW import lib,
  새 DLL 사본은 커밋 대상으로 포함한다.

## 이벤트와 수명

| 기존 역할 | GLFW 구현 | 주의점 |
| --- | --- | --- |
| 창/3.3 core 컨텍스트 | `GlfwWindow`가 생성·소유 | GLEW 초기화 전에 컨텍스트를 current로 설정 |
| 키 Down/Up, 특수 키 | `glfwSetKeyCallback` | 영문은 소문자 게임 키로 변환, Enter/Tab/Esc/키패드 +/- 유지 |
| 반복 입력 차단 | `GLFW_REPEAT` 무시 | ClearInput 이후에도 F9/F10/패널 키를 연타로 재실행하지 않음 |
| Ctrl+클릭 | 마우스 callback의 modifiers | 전역 키 상태 조회 대신 해당 이벤트의 Ctrl 상태 사용 |
| 마우스 선택 | cursor logical → framebuffer 좌표 | Windows 배율/모니터 DPI가 달라도 UI·선택 좌표 일치 |
| 리사이즈 | framebuffer-size callback | 실제 픽셀 크기 사용, 0 크기에는 FBO를 만들거나 그리지 않음 |
| 포커스 상실 | focus callback 및 Tick의 focus 확인 | WASD/방향키 고착 방지. 월드 업데이트 자체는 계속함 |
| 화면 교체 | `glfwSwapBuffers` | 기존 SwapMs 측정 위치 유지 |
| X/Esc 종료 | should-close → 루프 종료 → `Close` | 저장/모니터 종료/GL 해제 후 창·컨텍스트·GLFW 해제 |

OpenGL 헤더는 기존 GLEW가 소유한다. 게임 프로젝트에 `GLFW_INCLUDE_NONE`을 정의했고
정적 링크이므로 `GLFW_DLL`을 정의하지 않았다. GLFW 자체 프로젝트에는 내부 헤더 옵션을
외부에서 주입하지 않는다. HDR은 기존 RGBA16F 오프스크린 버퍼를 그대로 사용한다.
쿼터뷰 렌더러가 쓰지 않는 기본 창 depth/stencil/MSAA 버퍼를 요청하지 않는다.

## 프레임 스케줄링과 비용

### 기본 모드

기존 `glutTimerFunc(16, Tick, 0)`의 약 62.5 FPS 목표를 유지하는 16ms 소프트웨어 제한이다.
명시적으로 `swap_interval=0`을 요청한다. 기존 FreeGLUT 코드에는 VSync 명시 설정이 없었으므로
과거의 실제 swap interval은 드라이버/환경에 따라 다르며 동일했다고 단정하지 않는다.
VSync off에서는 tearing이 보일 수 있다. 필요하면 아래 `--vsync` 모드를 사용한다.

`다음 마감 = 프레임 시작 + 16ms`로 잡아 update/render/swap 비용 뒤에 16ms를 추가하지 않는다.
남는 시간은 `glfwWaitEventsTimeout`으로 이벤트를 받으며 대기한다. 마우스/키 이벤트로 일찍
깨어나도 마감 전에는 다음 시뮬레이션/렌더를 반복하지 않는다. 느린 프레임은 대기 없이 다음
프레임으로 넘어가며 누락 프레임 수만큼 업데이트를 몰아서 실행하지 않는다.
기존 Tick의 실측 dt와 50ms 상한, 일시정지/패널 규칙은 유지했다.

Windows backend가 대기 시간을 정수 밀리초로 내림하므로 1ms 미만의 나머지는 1ms로 대기한다.
0ms 대기를 반복하는 busy-spin을 피하는 대신 OS 스케줄링에 의한 작은 초과 대기는 허용한다.
기본 모드는 세션 동안 `timeBeginPeriod(1)`을 요청하고 성공했을 때 종료 시 `timeEndPeriod(1)`로
반납한다. 거친 시스템 타이머로 16ms 요청이 길어지는 위험을 줄이기 위한 것이다.
실패하면 경고를 기록하고 계속 동작한다. 정밀 타이머 요청은 CPU 전력/스케줄링 비용이 있으며,
Windows 11에서 완전히 가리거나 최소화된 창의 정밀도는 OS가 보장하지 않는다.
VSync/uncapped 모드에서는 이 타이머 요청을 하지 않는다.

### 선택 모드

- `SimpleGame.exe --vsync`: `swap_interval=1`, 소프트웨어 제한은 0. 화면 교체 대기와
  16ms 제한을 중첩하지 않는다. 화면 주사율이 60Hz라면 60 FPS 제한 자체는 성능 저하 증거가 아니다.
- `SimpleGame.exe --uncapped`: `swap_interval=0`, 소프트웨어 제한 0. 최대 처리량 측정용이며
  CPU/GPU 사용률이 올라갈 수 있다. 두 옵션을 동시에 주면 VSync를 우선한다.
- 최소화/0 framebuffer에서는 모든 모드에서 16ms 목표로 **월드 업데이트만** 계속한다.
  렌더/Swap은 생략하고 이벤트 대기를 사용한다. 창이 숨겨진 uncapped 모드의 빈 반복을 막는다.
  드라이버/OS 절전 정책에 따라 백그라운드 주기는 달라질 수 있다.

## 로깅과 사용자 성능 비교

기존 `[Render]` FPS와 glDraw 계열 draw call 계수, `[Profile]` 캐시/업로드/CPU render/swap을 유지했다.
`[WindowProfile]`과 성능 CSV schema 5에 다음을 추가했다. 프레임마다 콘솔을 출력하지 않고
기존처럼 약 1초 단위로 집계한다.

| 지표 | 의미 |
| --- | --- |
| `update_ms` | Tick 전체 CPU wall time. 월드 스트리밍/시뮬레이션/모니터링 포함 |
| `event_poll_ms` | 해당 렌더 프레임까지 GLFW poll 호출에 쓴 wall time |
| `event_wait_ms` | 이벤트 대기 호출의 wall time. **잠든 시간과 그 호출이 처리한 callback 시간 포함** |
| `swap_interval` | 요청한 0/1 정책. 드라이버 강제 설정을 직접 측정한 값은 아님 |
| `frame_limit_ms` | 요청한 16/0 소프트웨어 제한 |

`render_ms`에는 기존처럼 render/swap이 포함되며 Tick/소프트웨어 대기는 포함되지 않는다.
따라서 `event_wait_ms`를 순수 CPU 연산 비용으로 해석하거나 render_ms에 중복 합산하면 안 된다.
최소화 중에는 그리지 않으므로 렌더 로그 샘플도 생성되지 않는다. 복원 직후 FPS 구간은 숨긴
시간을 포함할 수 있으니 정상 창 상태의 안정 구간을 비교한다.
CSV context에 `window=glfw-3.5.1`, swap interval, frame limit을 남긴다.

사용자가 비교할 때:

1. 기준 커밋과 변경 커밋을 별도 폴더에서 같은 Release/x64 설정으로 빌드한다.
2. 동일 저장 스냅샷, 시드, 카메라/줌, 창 픽셀 크기, 모니터 배율, 후처리와 에셋을 사용한다.
   NPC 시뮬레이션도 진행되므로 가능한 같은 상태에서 시작한다. 기존 저장은 백업한다.
3. 같은 드라이버의 VSync 강제 설정/주사율/전원 정책을 유지한다. 시작 직후 에셋·캐시 생성
   구간은 따로 비교하고, 캐시가 안정된 뒤 30~60초 구간을 비교한다.
4. FPS 외에 draw calls, submitted vertices, upload bytes, mesh builds, cache misses,
   CPU render/swap ms와 프레임 최대 시간을 본다. 같은 장면에서 draw/mesh 작업이 증가했다면 조사한다.
5. `--uncapped`는 새 프레임워크의 처리량 측정용이다. 16ms 제한이 있는 이전 빌드의 FPS와
   숫자만 비교해 성능 향상이라고 보고하지 않는다. 신규 update/event 지표는 이전 schema에 없다.
6. WASD/방향키, Ctrl+클릭, 휠·키패드 줌, Tab/Q/T/F, F9/F10, 포커스 전환,
   최소화·복원, 서로 다른 DPI 모니터 이동, X/Esc 종료 후 저장을 확인한다.
7. 새 PC 또는 라이브러리 전역 경로를 설정하지 않은 환경에서 네 구성 빌드와 DLL 자동 복사를 확인한다.

완료한 정적 점검: 활성 FreeGLUT 참조 제거, 네 구성의 프로젝트 연결/파일 경로,
공식 GLFW 소스 목록/파일 무변경 확인, GLEW DLL 아키텍처·해시, include 순서,
이벤트/종료/가림·대기 경로, 코드 정렬 및 diff 공백 점검.
**실행 성능 무저하와 네 구성의 실제 빌드 성공은 사용자의 측정 전까지 미확인이다.**

## 공식 참고

- [GLFW 3.5.1 release](https://github.com/glfw/glfw/releases/tag/3.5.1)
- [GLFW compiling guide](https://www.glfw.org/docs/latest/compile_guide.html)
- [GLFW input/event guide](https://www.glfw.org/docs/latest/input_guide.html)
- [GLFW context/swap interval guide](https://www.glfw.org/docs/latest/context_guide.html)
- [Microsoft timeBeginPeriod](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod)
