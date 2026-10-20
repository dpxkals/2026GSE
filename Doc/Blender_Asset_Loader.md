# Blender 에셋 로더와 애니메이션 연결

기준 문서는 `assets/LowPoly_DarkAge_v1/README.md`다. 원본 `.blend`는 편집용으로 유지하고,
게임은 Blender에서 내보낸 GLB/glTF와 Data JSON을 읽는다. 게임 실행에 Blender나 Python은 필요 없다.
원본 에셋·README·기존 플레이 저장은 이번 구현에서 직접 수정하지 않았다.

## 현재 연결

- 모델 파일과 에셋 카탈로그를 `assets` 아래에서 재귀 탐색한다. 파일 개수를 78개로 제한하지 않는다.
- `asset_catalog.json`의 `asset_id/file`을 등록한다. 아직 카탈로그에 넣지 않은 GLB/glTF는 파일명에서 확장자를 뺀 ID로도 찾는다.
- 플레이어·안내 주민·동적 NPC에 모듈 파트 조합을 연결했다.
- 나무·바위·폐허·주택·수도원·내성·우물·묘비·성벽·망루를 설정에 따라 연결했다.
- 현재 팩에는 몬스터 GLB와 스킨/클립이 없다. 몬스터·영혼·봉화 등 미지정 외형은 기존 렌더러를 사용한다.
- NPC 210개 프리셋은 외형 레시피이며 실제 NPC ID로 간주하지 않는다.
  실행 중 실제 주민 ID를 읽어 `SaveData/<월드>/npc_appearance_bindings_v1.json`에 ID→레시피 연결을 저장한다.
  최초 연결은 소속 마을을 우선하고, 이사 후에는 기존 연결을 유지한다.
  사망 후 교체된 ID는 다음 동기화에서 새 레시피를 배정한다. 동기화 간격은 5초다.
- 명시적인 `runtime_npc_id`가 있는 레시피는 해당 연결을 우선한다. 중복 ID/레시피는 거부한다.
  파일의 ID는 64비트 정밀도를 지키기 위해 문자열로 저장한다.

## 계속 추가/수정하는 흐름

1. Blender 원본에서 작업하고 런타임 파일을 GLB 또는 glTF+bin으로 내보낸다.
2. 새 파일을 `assets` 아래에 추가한다. 파트라면 `npc_parts.json`에 새 인덱스를 **뒤에 추가**하고,
   프리셋에서 선택한다. 기존 슬롯 인덱스와 레시피 ID는 유지한다.
3. 게임 오브젝트에 쓸 ID를 `SimpleGame/Config/AssetBindings.json`에 지정한다.
4. 게임에서 인벤토리/퀘스트 패널을 닫고 **F10**을 누른다.

개발 환경에서는 프로젝트의 원본 `assets`와 `SimpleGame/Config/AssetBindings.json`을 우선 읽는다.
따라서 실행 파일을 다시 빌드하지 않아도 모델/팔레트/프리셋/연결 설정을 F10으로 갱신할 수 있다.
새 C++ 기능을 추가했을 때는 게임을 다시 빌드해야 한다.

배포 환경에서는 실행 파일 옆 `assets`와 `Config/AssetBindings.json`을 읽는다.
빌드 시 GLB/glTF/bin/JSON/README와 설정/라이선스 안내를 자동 복사한다. `.blend`와 미리보기 PNG는 복사하지 않는다.
별도 경로를 쓸 때는 `SimpleGame.exe --assets "D:\내 에셋 폴더"`를 사용한다. 한글 경로를 지원한다.

F10은 메타데이터를 임시 상태에 읽고 기본 계약이 유효할 때 교체한다. 실패하면 현재 런타임을 유지한다.
슬롯/레시피 외에도 연결 설정의 배율·회전·변형 목록·클립 이름 목록과 NPC ID 형식을 미리 확인한다.
개별 GLB는 첫 사용 때 읽는다. 해당 모델이 손상되었거나 지원하지 않는 형식이면 오류를 한 번 기록하고
그 오브젝트에 기존 외형을 사용한다. 다시 시도하려면 파일을 고친 뒤 F10을 누른다.
자동 파일 감시는 추가하지 않았다. 내보내기를 끝낸 뒤 F10을 눌러 부분 저장 파일을 읽지 않도록 한다.

README는 제작/연동 계약 문서다. 런타임이 Markdown 설명을 실행하거나 해석하여 코드를 바꾸지는 않는다.
현재 JSON `schema_version: 1`의 필드 추가와 변형 추가는 수용한다. 기존 필드의 의미/좌표계가 바뀌는 큰 변경은
스키마를 올리고 로더 이행 코드를 함께 수정해야 한다. 이미 저장된 레시피 ID를 제거하면 조용히 다른 외형으로 바꾸지 않고 알려준다.

## 설정 예

`SimpleGame/Config/AssetBindings.json`은 제작 데이터와 게임 배치 사이의 연결 파일이다.

```json
{
  "roles": {
    "player": { "model": "CHAR_Traveler_Rigged", "scale": 0.8 },
    "husk": { "model": "MON_Husk", "scale": 0.8 },
    "hound": { "model": "MON_Hound", "scale": 1.0 },
    "boss": { "model": "MON_AshenGuardian", "scale": 1.5 }
  }
}
```

위 코드는 전체 설정을 대체하는 파일이 아니라 `roles` 안에서 바꿀 항목의 예다.
현재 실제 파일에는 아직 없는 몬스터를 빈 ID로 두었다. ID와 일치하는 파일을 추가한 뒤 지정하면 된다.
플레이어/안내 주민은 완성 `model` 또는 `recipe`를 선택한다. 동적 주민의 완성 리깅 모델은 `npc_model`로 지정한다.
`npc_model`이 빈 문자열이면 저장된 레시피의 파트 조합을 사용한다.

- `npc_pack`: 파트 규격이 있는 팩의 상대 경로.
- `npc_presets`: 팩 안의 프리셋 JSON 상대 경로. 파일 이름에 든 210이라는 숫자를 로더가 주민 ID로 사용하지 않는다.
- `props.<종류>.model` 또는 `variants`: 구조물/자연물 모델 ID와 변형 목록.
- `yaw_degrees`, `scale`: 회전/배율.
- `fit_footprint`: 기존 타일 충돌 크기에 맞춰 지나치게 큰 모델의 가로 크기를 축소한다.
- `roof_palette`: `MAT_Roof`에 마을별 지붕 색상 적용. `MAT_Thatch/MAT_Leaf`는 원래 재질을 유지한다.
- `animation_aliases`: 게임의 `idle/walk/attack/hurt` 상태에 대응하는 실제 클립 이름 목록.

한 파일명/에셋 ID를 서로 다른 파일에 중복 사용하지 않는다. 여러 팩을 함께 쓸 때는 ID에 팩 접두사를 붙인다.

## 파트·좌표·색상 계약

- GLB는 +Y 위인 표준 glTF 좌표로 읽는다. Blender의 `(x,y,z)`는 내보낼 때 `(x,z,-y)`가 된다.
- 게임 투영에서는 glTF `(x,y,z)`를 월드 평면 `(x/2,-z/2)`와 높이 `y`로 사용한다. 1타일=2m 기준이다.
- **개별 파트를 바운딩 박스 중심으로 옮기지 않는다.** 모든 파트를 공통 발밑 원점으로 겹친다.
- 손 장비에 소켓 좌표를 다시 더하지 않는다. 현재 정적 GLB는 이미 손 위치에 맞춰져 있다.
- `body/head/torso/legs`는 필수, 선택 파트의 -1은 미장착이다.
- 후드/투구와 머리, 화살통과 긴 머리, 로브와 가방 등의 제한은 `compatibility` 데이터로 확인한다.
- 스킨/머리/옷은 이름 있는 재질 슬롯에 선형 RGB 팔레트를 적용한다.
  투영 결과를 기존 렌더러에 넘길 때 표시 RGB로 변환하고, HDR 경로에서 다시 선형화해 중복 감마를 피한다.
- 메시의 금속/거칠기 값은 보관하지만 현재 화면은 단색 재질의 간단한 방향광/발광 표현이다. 완전한 PBR 셰이딩은 아니다.
- 원본 모델의 충돌 메시를 새로 만들지 않는다. 충돌·사거리·전투 판정은 기존 게임 데이터를 사용한다.

## 스켈레탈 메시/애니메이션

로더/재생기는 다음을 구현한다.

- 노드의 parent/child 계층, TRS 및 정적 행렬 변환.
- skin의 joint 목록과 inverse bind matrices.
- `JOINTS_0/WEIGHTS_0`, 선택적인 `JOINTS_1/WEIGHTS_1`: 정점당 최대 8개 영향, 스킨당 최대 256 joints.
- 역바인드가 없을 때 항등 행렬. 웨이트 합 정규화와 잘못된 인덱스/빈 웨이트 검출.
- glTF 애니메이션의 이동·회전·스케일 채널.
- STEP, LINEAR, CUBICSPLINE. 회전 LINEAR는 quaternion slerp, cubic 회전 결과는 정규화한다.
- 반복/단발 재생, 재생 속도, 명시적 재시작, 끝 판정.
- idle/walk 루프, attack/hurt 이벤트 단발 재생과 새 이벤트에서 재시작.
- 게임 시뮬레이션 시계를 사용하므로 P/인벤토리 일시정지 때 포즈가 멈춘다. F9 시간 초기화 때 런타임 포즈도 비운다.
- 이름 있는 본/소켓 노드의 현재 행렬을 `Models::NodeTransform`으로 얻을 수 있다.

스키닝 결과는 `globalJoint * inverseBind`를 웨이트로 합산한 행렬로 계산한다.
현재 브리지는 CPU에서 포즈를 계산하고 기존 2.5D 배치 렌더러에 투영한다. GPU 스키닝은 추가하지 않았다.
스켈레톤/토폴로지는 매 프레임 다시 읽거나 만들지 않고, 재생 인스턴스만 상태를 갖는다.

Blender에서 리깅 파일을 내보낼 때:

1. 필요한 메시와 Armature를 함께 내보내고 Skinning/Animation을 활성화한다.
2. 액션/클립 이름을 `Idle`, `Walk`, `Attack`, `Hit` 등으로 정하거나 aliases 설정을 수정한다.
3. 공통 원점과 적용된 배율을 유지하고, 이동 애니메이션은 제자리(in-place)로 제작한다.
4. 파트별 파일을 계속 쓸 경우 같은 본 이름·바인드 포즈·타이밍을 유지하고, 각 파일에 필요한 스킨/클립을 포함한다.
   서로 다른 골격을 자동으로 리타게팅하지 않는다. 완성 캐릭터 GLB 하나를 연결하는 것도 가능하다.

현재 제공된 78개 GLB는 정적 A 자세이며 실제 스킨과 클립이 0개다.
따라서 이 팩을 읽는 것만으로 보행 본 애니메이션이 새로 생기지는 않는다. 현 시점에는 방향/호흡/이동 흔들림과 기존 전투 효과가 적용된다.
실제 애니메이션 품질과 본 접합은 리깅된 GLB가 추가된 뒤 확인해야 한다.

## 캐시·로깅·렌더링 한계

- 원본 모델은 파일별로 한 번 읽고 immutable shared data로 사용한다. 카탈로그 ID와 파일명 별칭이
  같은 파일을 가리켜도 중복 디코딩하지 않는다. 손상된 파일 역시 F10 전에는 반복해서 읽지 않는다.
- 정적 조합은 모델 내용 해시+메타데이터+팔레트+방향 키로 기존 메모리/디스크 메시 캐시에 저장한다.
  프레임 시간·NPC ID·포즈 각도를 정적 캐시 키에 넣지 않는다.
- 애니메이션은 재사용되는 포즈/정점 작업 공간을 갱신하고 기존 삼각형 배치 경로로 제출한다.
  포즈마다 새 캐시 파일이나 VAO/VBO를 만들지 않는다.
- F10에서 가져온 모델용 메모리 캐시만 비운다. 디스크의 구버전 캐시는 남으며 원본 해시가 바뀌면 새 키를 쓴다.
- 모델 내부는 면 깊이 정렬, 오브젝트 간은 기존 SceneGraph 깊이 정렬이다.
  완전한 3D 깊이 버퍼 기반 교차 가림이나 정밀 투명 정렬을 보장하지 않는다.
- `[Assets]`, `[AssetLoad]`, `[AssetBindings]` 콘솔에 경로·파일 수·프리셋 수·삼각형·본/클립 수·로딩 시간을 기록한다.
- 성능 CSV schema 4에 `asset_loads`, `asset_decode_ms`, `skin_vertices`, `asset_pose_ms`를 추가했다.
  기존 draw/FPS/캐시/업로드 지표와 함께 CPU 스키닝 비용을 볼 수 있다. 성능 향상 수치는 측정하지 않았다.

현재 지원하지 않는 입력은 텍스처 재질, Draco/meshopt 압축, morph target, mesh GPU instancing 확장,
점/선 primitive 및 필수 확장이 알 수 없는 glTF다. 조용히 잘못 읽지 않고 오류를 남긴다.
크로스페이드/애니메이션 블렌딩/리타게팅/루트 모션의 실제 Actor 이동/IK는 별도 기능이다.
개별 파일과 전체 로드 입력은 256MiB, 정점 합계는 100만, 애니메이션 출력 샘플 합계는 100만으로 제한한다.
외부 `.bin` 참조는 assets 루트를 벗어날 수 없다.

## 파일 구성과 확인

- `ModelAsset.h`, `GltfLoader.cpp`: OpenGL과 독립된 모델/본/클립 데이터와 파서.
- `ModelMath.cpp`, `ModelAnimation.cpp`: 변환·보간·CPU 스키닝·개체 재생 상태.
- `ModelDrawing.h/.cpp`: 단색 재질과 모델 내부 정렬, 기존 배치 렌더러 연결.
- `AssetRuntime.h/.cpp`: 탐색·JSON 계약·프리셋/실제 ID 연결·지연 로딩·F10 교체.
- `Config/AssetBindings.json`: 에셋과 게임 타입 연결.
- `Dependencies/ASSET_LOADER_LICENSES.md`: 고정 버전 파서/JSON 라이브러리 출처와 라이선스.
- `Tests/AssetLoaderTests.cpp`: 변환·inverse transpose normal·8영향 스킨·이중 mesh transform 방지·선형/단계/cubic/회전 보간·단발/속도/재시작의 참조값 검사와 실제 파일 로딩용 선택 테스트.

빌드·게임 실행·테스트 실행·화면 확인은 사용자 지시에 따라 수행하지 않았다.
소스/프로젝트 연결과 현재 에셋의 파일 구조·JSON 계약을 확인했다. 실제 GPU 동작과 리깅 파일 호환성은 미확인이다.

사용자가 별도로 CPU 테스트를 실행하려면 프로젝트 루트의 Visual Studio Developer PowerShell에서:

```powershell
New-Item -ItemType Directory -Force Tests/bin
cl /nologo /std:c++17 /EHsc /utf-8 /I SimpleGame Tests/AssetLoaderTests.cpp SimpleGame/GltfLoader.cpp SimpleGame/ModelMath.cpp SimpleGame/ModelAnimation.cpp /Fe:Tests/bin/AssetLoaderTests.exe
./Tests/bin/AssetLoaderTests.exe ./assets
```

게임 확인 순서: 새 모델 외형/조합/색상 → 움직임/가림 → 같은 ID로 재접속 → 파일 수정 후 F10 →
잘못된 파일에서 대체 외형/오류 로그 → 리깅 파일의 Idle/Walk/Attack/Hit → 210명 환경의 CPU/캐시 로그.

참고: [glTF 2.0 규격](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html),
[cgltf v1.15](https://github.com/jkuhlmann/cgltf/blob/v1.15/cgltf.h),
[JSON for Modern C++ v3.11.3](https://github.com/nlohmann/json/tree/v3.11.3).
