# Actor / SceneGraph 구조

## 역할

- `SimpleGame/Actor.h/.cpp`: 오브젝트 ID, 위치 바인딩, 활성화/표시 상태, 업데이트와 제거 콜백을 제공한다.
- `SimpleGame/SceneGraph.h/.cpp`: Actor 소유권, 부모·자식 관계, 위치 전파, 업데이트 단계, 렌더 레이어 및 깊이 정렬을 관리한다.
- `SimpleGame/LevelOneActors.cpp`: 기존 게임 데이터와 Actor를 안정적인 레코드 ID로 연결한다. 벡터 원소 주소를 보관하지 않는다.
- `SimpleGame/GameplayBehaviors.h/.cpp`: 적 이동/공격, 발사체 충돌, 아이템 흡인 동작을 제공한다.
- `SimpleGame/LevelOnePlayer.cpp`, `LevelOneProgression.cpp`, `LevelOneSpawning.cpp`, `LevelOneResolution.cpp`: 플레이어, 성장, 스폰, 전투 결과 처리를 분리한다.
- `SimpleGame/WorldActors.h/.cpp`: 스트리밍 청크와 구조물의 원본 좌표를 보관한다.
- `SimpleGame/SimpleGame.cpp`: 기존 OpenGL 렌더러를 Actor 타입별로 등록하고 씬 그래프를 그린다.

Actor와 SceneGraph의 인터페이스는 OpenGL 및 LevelOne에 의존하지 않는다. 기존 저장 데이터는 게임 상태의 원본이며 Actor는 이를 바인딩하는 런타임 오브젝트다. 모든 게임 데이터를 Actor 내부로 이전하는 ECS 구조는 아니다.

## 기본 트리

```text
SceneGraph
  world
    terrain (청크)
      prop (구조물)
        beacon-glow
  gameplay
    player / enemy / projectile / drop / soul / number
  residents
    npc
  overlay
  hud
```

지형은 타일마다 Actor를 만들지 않고 청크 단위 Actor에서 기존 캐시 메시를 그린다. 캠프 표시와 범위 표시 등의 보조 효과도 묶음 Actor로 처리한다.

## 사용 규칙

```cpp
Actor& parent = scene.Ensure("example", "group");
Actor& child = scene.Ensure("example/child", "custom", parent.GetId());
scene.SetLocalPosition(child.GetId(), {1.0, 0.0});
scene.SetPosition(parent.GetId(), {10.0, 20.0});
// child의 월드 위치는 (11, 20).
```

새 타입은 `SetRenderer`로 그리기 함수를 등록하고 `BindUpdate` 또는 Actor의 가상 Update로 동작을 연결한다. 기본 업데이트 단계는 Passive다.

- 같은 단계 안에서는 부모를 먼저 업데이트한다.
- active=false는 자식 업데이트도 중지한다. visible=false는 자식 표시도 숨긴다. 두 상태는 독립적이다.
- 월드 위치 변경은 SetPosition, 부모 기준 위치 변경은 SetLocalPosition을 사용한다.
- Reparent는 기본적으로 월드 위치를 유지하며 순환 연결을 거부한다.
- Destroy는 자식까지 제거하고 제거 콜백을 호출한다. 순회 중 제거는 순회 종료까지 지연된다.
- Retain은 스트리밍/동기화에서 런타임 Actor만 해제한다. 제거 콜백을 호출하지 않아 저장된 월드 데이터는 남는다.
- 렌더 레이어는 Ground → GroundOverlay → World → Effects → UI 순서다. World는 쿼터뷰 깊이로 정렬한다.
- 부모/자식 이동을 함께 반영하려면 외부에서 저장 레코드 위치를 직접 바꾸지 말고 SceneGraph의 위치 API를 사용한다.

## 현재 범위와 제한

기존 저장 포맷은 유지한다. 발사체/피해 숫자의 runtimeId, 씬 트리와 활성화/표시 상태는 저장하지 않는다. 기존 고정 NPC의 이동 좌표도 현재 NPC 저장 포맷에 포함되지 않는다.

부모 변환은 평행이동만 지원하며 회전/스케일 상속은 없다. 정적 지형과 구조물은 원본 월드 좌표의 충돌 데이터를 사용하므로 Actor 위치 변경만으로 충돌 격자가 이동하지 않는다. 현재 게임에서는 정적 월드 Actor를 이동시키지 않는다.

게임 규칙과 저장 모델은 LevelOne 계층에 남아 있다. 이 구조는 기존 게임을 보존하면서 재사용 가능한 오브젝트/씬 관리 기반을 도입하는 첫 단계다.

## 사용자 확인 항목

요청에 따라 빌드, 실행, 테스트 및 렌더링 결과 확인은 수행하지 않았다.

1. Visual Studio에서 빌드 후 플레이어 이동과 자동 공격을 확인한다.
2. 적/보스, 경험치, 아이템 습득과 자석 효과를 확인한다.
3. 사망 시 영혼 NPC 생성, 부활 및 기존 저장 불러오기를 확인한다.
4. 청크 이동 시 지형/구조물 생성·해제, 메시 캐시와 가림 반투명 효과를 확인한다.
5. 한글 대화, 피해 숫자, HUD, HDR 후처리를 확인한다.
6. 추가 Actor로 부모 이동, 자식 상대 위치, 활성화/표시 상속 및 업데이트 중 제거를 확인한다.
