# 오픈월드 RPG 통합 게임 기획서

> 파일명: `OpenWorld_RPG_Integrated_Design.md`  
> 목적: 지금까지 논의한 스토리, 전투, 성장, NPC AI, 퀘스트, 월드 시뮬레이션, AI LOD, 인구 관리 시스템을 하나의 개발 참고 문서로 통합한다.  
> 상태 표기:
> - **핵심 유지**: 현재 기획에서 계속 유지하기로 한 방향
> - **구현 제안**: 실제 개발 시 권장하는 구조
> - **스토리 후보**: 아직 최종 확정되지 않은 세계관/반전 아이디어

---

# 1. 게임 기본 방향

## 1.1 장르 및 시점

- 장르: RPG / 오픈월드
- 그래픽: 2.5D
- 시점: 쿼터뷰
- 배경: 중세 암흑시대 기반 다크 판타지
- 분위기: 전체적으로 암울하지만 작은 도움, 신뢰, 공동체 회복 등을 통해 희망이 드러나는 세계
- 월드: 청크 기반으로 확장되는 오픈월드
- 핵심 특징:
  - 몬스터 사냥
  - 파밍
  - 경험치 및 레벨업
  - 보스 전투
  - 원거리 자동 에이밍
  - 플레이어와 NPC 사이의 전투
  - NPC의 사냥 / 거래 / 약탈 / PK
  - 동적 NPC AI
  - AI LOD
  - 메인 퀘스트 / 서브 퀘스트
  - 플레이어 사망과 NPC 계승
  - 영속적인 월드 변화

---

# 2. 핵심 플레이 루프

```text
지역 탐험
↓
몬스터 / NPC / 퀘스트 발견
↓
원거리 자동 에이밍 전투
↓
몬스터 처치 / 파밍
↓
경험치 획득
↓
레벨업 / 장비 성장
↓
강한 지역 진입
↓
보스 출현
↓
보스 전투
↓
스토리 / 월드 상태 변화
↓
새로운 지역 탐험
↓
반복
```

플레이어 사망 시에는 별도의 핵심 루프가 발생한다.

```text
플레이어 사망
↓
사망 직전 플레이 기록 분석
↓
성격 / 행동 / 감정 성향 추출
↓
계승자 NPC 생성
↓
월드에 영구 기록
↓
플레이어 부활
↓
게임 계속 진행
```

---

# 3. 세계관 핵심

## 3.1 죽음과 범람

오래전 이 세계에서 죽음은 삶의 끝이었다.

그러나 어느 시점부터 일부 인간이 죽은 뒤 다시 살아나는 현상이 발생하기 시작한다.

처음에는 이를 신의 기적이라고 여겼지만, 시간이 지나면서 이상 현상이 증가한다.

- 전에 없던 몬스터가 나타난다.
- 존재하지 않던 숲, 폐허, 지역이 생겨난다.
- 사람들의 기억과 실제 지형이 어긋난다.
- 죽음 이후 새로운 인간이 나타나기도 한다.

사람들은 이러한 현상을 **「범람」**이라고 부른다.

```text
죽음
├─ 새로운 인간
├─ 몬스터
└─ 새로운 공간 / 지역
```

> **스토리 후보:** 위 세 현상이 실제로 하나의 세계 법칙에서 발생한다는 방향은 현재 이야기의 중심 미스터리 후보이다. 구체적인 진실은 아직 최종 확정하지 않는다.

---

# 4. 플레이어 시작 상태

```yaml
Player:
  Level: 1
  Status: Human
  Resurrection: Unknown
  AutoAim: Enabled
  MonsterDetection: Unknown
  Memory: Normal
```

플레이어는 처음에는 자신이 특별한 존재라는 사실을 알지 못한다.

---

# 5. 전투 시스템

## 5.1 기본 전투

플레이어의 기본 공격은 **원거리 공격**이다.

플레이어는 이동과 회피에 집중하고, 공격 방향은 자동 에이밍이 담당한다.

```text
Target Detection
↓
Target Selection
↓
Auto Aim
↓
Projectile Attack
↓
Hit
↓
Damage
```

---

## 5.2 자동 에이밍

기본적으로 적대 몬스터를 자동 감지한다.

추천 타게팅 우선순위:

```text
1. 현재 플레이어를 공격 중인 대상
2. 플레이어가 명시적으로 선택한 대상
3. 가까운 적대 몬스터
4. 가까운 적대 NPC
```

중립 NPC는 자동 타게팅하지 않는다.

---

## 5.3 NPC 공격

플레이어는 일반 NPC도 공격할 수 있다.

중립 NPC 공격 방식:

```text
마우스 좌클릭
↓
Cursor Hit Test
↓
NPC 선택
↓
Combat Target 설정
↓
자동 에이밍 활성화
↓
원거리 공격
```

핵심 원칙:

- 몬스터: 자동 타게팅 가능
- 적대 NPC: 자동 타게팅 가능
- 중립 NPC: 직접 좌클릭해야 공격 대상이 됨
- 퀘스트 NPC: 별도 공격 제한 규칙 적용 가능

---

## 5.4 공통 Combat Target 구조

몬스터와 NPC를 서로 다른 전투 시스템으로 만들지 않고 공통 인터페이스로 처리한다.

```cpp
class ICombatTarget
{
public:
    virtual FVector GetTargetPosition() = 0;
    virtual bool IsAlive() = 0;
    virtual bool CanBeTargeted() = 0;
    virtual void TakeDamage(float Damage) = 0;
};
```

```text
ICombatTarget
├─ Monster
├─ Boss
├─ Dynamic NPC
└─ 기타 공격 가능한 대상
```

---

# 6. 자동 에이밍의 세계관적 의미

게임 초반에는 단순한 특수 능력처럼 보인다.

> 플레이어는 몬스터의 기척을 감지하고 공격을 자동으로 맞출 수 있다.

중후반에는 다음 가능성을 제시한다.

> **스토리 후보:** 플레이어가 감지하는 것은 단순히 몬스터가 아니라 플레이어와 같은 근원을 공유하는 존재일 수 있다.

이 설정은 플레이어와 몬스터의 기원을 연결하는 핵심 떡밥으로 활용 가능하다.

---

# 7. 몬스터 및 파밍

## 7.1 몬스터

세계의 일반 주민들은 범람 이후 나타난 위험한 존재들을 몬스터라고 부른다.

몬스터는 마을 외곽, 숲, 폐허, 던전 등에 지속적으로 출몰한다.

---

## 7.2 잔흔

몬스터를 처치하면 몸이 붕괴하며 특수한 힘이나 물질이 남는다.

명칭:

**「잔흔」**

```text
Monster Death
↓
Body Collapse
↓
Remnant / 잔흔
↓
Drop Table
├─ Weapon
├─ Armor
├─ Material
├─ Consumable
└─ Unique Item
```

몬스터가 아이템을 떨어뜨리는 현상을 세계관과 연결한다.

---

# 8. 경험치 및 레벨업

몬스터가 죽을 때 잔흔 일부는 플레이어에게 흡수된다.

이것을 게임 시스템에서는 경험치로 표현한다.

```text
Monster Kill
↓
Remnant Absorption
↓
EXP Gain
↓
EXP Threshold
↓
Level Up
```

세계관상 플레이어는 다른 존재의 힘 일부를 흡수하며 강해진다.

핵심 미스터리:

> 왜 일반 인간이 아닌 플레이어만 이런 방식으로 빠르게 성장하는가?

---

# 9. 초반 스토리 진행

## 9.1 Chapter 0 - 평범한 인간

플레이어는 변경 지역에서 Level 1로 시작한다.

```yaml
Area:
  Name: 변경 마을
  RecommendedLevel: 1
  MonsterLevel:
    Min: 1
    Max: 3
```

최근 마을 주변에서 다음 현상이 발생한다.

- 전에 없던 몬스터 출현
- 새로운 숲 발견
- 사람들이 기억하지 못하는 폐허 발견
- 오래된 지도와 현재 지형 불일치

---

## 9.2 첫 전투

```text
첫 몬스터 조우
↓
자동 에이밍 튜토리얼
↓
몬스터 처치
↓
첫 아이템 획득
↓
첫 경험치 획득
```

---

## 9.3 초반 성장

```text
LEVEL 1
↓
몬스터 사냥
↓
파밍
↓
LEVEL 2
↓
새 장비
↓
LEVEL 3
```

초반에 학습하는 시스템:

- 이동
- 자동 에이밍
- 공격
- 회피
- 아이템 획득
- 장비
- 경험치
- 레벨업

---

# 10. 첫 미스터리

마을 주민들의 기억이 서로 다르다.

예시:

```text
NPC A:
"저 괴물들은 최근에 나타났어."

NPC B:
"무슨 소리야? 저 숲에는 옛날부터 괴물이 있었는데."
```

그리고 오래된 지도에는 현재의 숲이 존재하지 않는다.

플레이어가 가지게 되는 첫 질문:

> 세계 자체가 변하고 있는 것인가?

---

# 11. 첫 던전

## 존재하지 않았던 폐허

```yaml
Dungeon:
  RecommendedLevel:
    Min: 3
    Max: 5
```

최근까지 존재하지 않았던 장소에 갑자기 폐허가 나타난다.

```text
Dungeon Enter
↓
Monster Battle
↓
Item Farming
↓
Lore Discovery
↓
Elite Monster
↓
Boss Room
```

---

# 12. 첫 보스

## 태어나지 못한 것

```yaml
Boss:
  Name: 태어나지 못한 것
  RecommendedLevel: 5
```

죽기 직전 보스가 플레이어에게 말한다.

> "또... 너인가."

플레이어는 이 존재를 처음 만났지만 보스는 플레이어를 아는 듯 행동한다.

---

# 13. 첫 죽음과 부활

첫 보스를 처치한 뒤 플레이어는 더 강한 존재와 조우하고 스토리상 첫 사망을 경험한다.

```text
Boss Clear
↓
귀환
↓
Unknown Entity Encounter
↓
Player Death
↓
Fade Out
↓
Resurrection
```

부활 후:

- 레벨 유지
- 일부 능력 유지
- 세계 상태 변화
- 계승자 NPC 생성

---

# 14. 계승자 NPC

플레이어가 죽을 때 새로운 NPC 하나가 생성된다.

계승자는 플레이어의 복제 인간이 아니다.

플레이어 사망 이전의 성격, 행동 패턴, 감정 성향 일부를 초기값으로 물려받은 **독립된 인간**이다.

```yaml
SuccessorNPC:
  SourceDeathID: PlayerDeathID

  Personality:
    Aggression: Value
    Altruism: Value
    Greed: Value
    Curiosity: Value
    RiskTaking: Value
    Mercy: Value

  Memory:
    PlayerMemory: Limited

  Relationship:
    Player: Unknown
```

생성 이후에는 자기 경험에 따라 성격과 Weight가 변화한다.

---

# 15. 플레이어 행동 기록

```yaml
PlayerTraits:
  Aggression: 0
  Altruism: 0
  Greed: 0
  Curiosity: 0
  RiskTaking: 0
  Mercy: 0
```

예:

```text
Help NPC
→ Altruism +

Attack Neutral NPC
→ Aggression +

Share Item
→ Altruism +

Take Excess Reward
→ Greed +

Discover Unknown Area
→ Curiosity +

Enter High Level Area
→ RiskTaking +

Spare Enemy
→ Mercy +
```

---

# 16. Death Record

```yaml
DeathRecord:
  DeathID: UniqueID
  PlayerLevel: 12
  DeathLocation: RegionID
  CauseOfDeath: MonsterID

  PlayerTraits:
    Aggression: 30
    Altruism: 75
    Greed: 20
    Curiosity: 90
    RiskTaking: 80

  SuccessorGenerated: true
  SuccessorNPCID: NPC_00125
```

핵심 규칙:

```text
1 Player Death
=
1 Successor NPC
```

---

# 17. 중반 이후 스토리 방향

```text
새로운 지역 탐험
↓
강한 몬스터
↓
파밍 / 레벨업
↓
계승자 NPC 발견
↓
죽음과 생명의 관계 조사
↓
말하는 몬스터
↓
과거 불사자 기록
↓
세계 확장 현상
↓
자동 에이밍의 진실
↓
몬스터 / 플레이어 / 계승자의 관계
```

> **스토리 후보:** 죽음, 몬스터, 계승자, 새로운 지역 생성이 동일한 세계 법칙과 연결되어 있다는 방향.

---

# 18. 레벨별 지역 구조 예시

| Level | Area | 주요 내용 |
|---|---|---|
| 1~5 | 변경 마을 | 범람 현상 발견 |
| 5~10 | 검은 숲 | 몬스터의 기원 조사 |
| 10~20 | 몰락한 도시 | 과거 불사자 기록 |
| 20~30 | 전쟁 지역 | 계승자 NPC와 세력 갈등 |
| 30~40 | 고대 연구 시설 | 죽음/부활 연구 |
| 40~50 | 세계의 경계 | 세계 변화의 원리 조사 |
| 50+ | 최종 지역 | 핵심 미스터리 해결 |

---

# 19. NPC 분류

```text
NPC
├─ Quest NPC
│  ├─ Main Quest NPC
│  └─ Sub Quest NPC
│
├─ Dynamic NPC
│  ├─ 사냥
│  ├─ 거래
│  ├─ 약탈
│  ├─ PK
│  ├─ 탐험
│  ├─ 성장
│  └─ AI LOD 적용
│
└─ Successor NPC
   ├─ Dynamic NPC 기반
   └─ 플레이어의 죽음에서 생성
```

---

# 20. Dynamic NPC AI 전체 구조

NPC 행동 선택은 다음 구조를 사용한다.

```text
Personality
↓
Current State
↓
Environment Context
↓
Fuzzy Rule Evaluation
↓
Dynamic Action Weight
↓
Weighted Random
↓
Selected Action
↓
Behavior Tree
↓
실제 행동
```

역할 분리:

- Personality = 어떤 NPC인가?
- Fuzzy / Utility = 지금 무엇을 하고 싶어 하는가?
- Weight = 행동 선택 가능성
- Weighted Random = 최종 행동 선택
- Behavior Tree = 선택된 행동을 어떻게 수행하는가?

---

# 21. NPC Personality

```cpp
struct Personality
{
    float Aggression;      // 공격성
    float Greed;           // 탐욕
    float Sociability;     // 사회성
    float Courage;         // 용기
    float Morality;        // 도덕성
    float Curiosity;       // 탐험 성향
};
```

기본 범위:

```text
0.0 ~ 1.0
```

예:

```text
NPC A

Aggression  = 0.8
Greed       = 0.7
Sociability = 0.2
Courage     = 0.9
Morality    = 0.1
```

→ PK / 약탈 성향이 강한 NPC

---

# 22. NPC 행동 종류

현재 핵심 행동:

```text
Hunt
Trade
Loot
PK
Explore
Rest
Flee
```

추후 확장 가능:

```text
Escort
Guard
Gather
Craft
Travel
Help
Revenge
JoinGroup
```

---

# 23. 기본 Action Weight

```cpp
struct ActionWeights
{
    float Hunt;
    float Trade;
    float Loot;
    float PK;
    float Explore;
    float Rest;
    float Flee;
};
```

예시 초기값:

```text
Hunt      30
Trade     20
Loot      10
PK         5
Explore   20
Rest      10
Flee       5
```

이 값은 최종 확률이 아니며, 현재 상황과 경험에 따라 계속 변화한다.

---

# 24. Fuzzy Rule 예시

## 24.1 PK

```text
IF Aggression HIGH
AND Morality LOW
AND TargetPower LOW
AND TargetLoot HIGH

THEN PK Weight HIGH
```

단순 계산식 예시:

```cpp
PKScore =
    Aggression *
    (1.0f - Morality) *
    TargetWeakness *
    TargetLootValue;
```

---

## 24.2 약탈

```cpp
LootScore =
    Greed *
    LootValue *
    Courage *
    (1.0f - Danger);
```

---

## 24.3 사냥

```cpp
HuntScore =
    HungerForGrowth *
    Courage *
    MonsterSuitability *
    (1.0f - Danger);
```

`MonsterSuitability`는 NPC와 몬스터의 전투력 차이를 반영한다.

---

## 24.4 거래

```cpp
TradeScore =
    Sociability *
    NeedForItem *
    MoneyAvailability *
    TraderTrust;
```

---

# 25. Weight의 실시간 변화

NPC의 경험이 행동 성향에 영향을 준다.

예:

```text
사냥 성공
→ Hunt +3

사냥에서 큰 피해
→ Hunt -5
→ Rest +4
→ Flee +2

PK 성공
→ PK +5
→ Loot +2

PK 실패
→ PK -8
→ Flee +5

거래 성공
→ Trade +2

거래에서 손해
→ Trade -4
→ Trust 감소
```

---

# 26. 다른 NPC의 행동이 Weight에 미치는 영향

NPC A가 NPC B의 행동을 목격했다고 가정한다.

```text
NPC B ──PK──> NPC C
       ↑
     NPC A가 목격
```

평화적 NPC A:

```text
PK Weight      -5
Flee Weight    +10
Trade Weight   -3
DangerLevel    +20
```

공격적 NPC A:

```text
PK Weight      +5
Loot Weight    +3
```

같은 사건도 Personality에 따라 다르게 해석한다.

---

# 27. Weighted Random

예:

```text
Hunt      40
Trade     25
Loot      15
PK        10
Explore    5
Rest       5
```

총합:

```text
100
```

랜덤 값:

```cpp
float Random = RandomRange(0.0f, 100.0f);
```

누적 Weight:

```text
0  ~ 40   Hunt
40 ~ 65   Trade
65 ~ 80   Loot
80 ~ 90   PK
90 ~ 95   Explore
95 ~ 100  Rest
```

장점:

- 매번 최고 Weight만 선택하지 않음
- NPC의 행동에 예측 불가능성이 생김
- 같은 성격의 NPC라도 서로 다른 삶을 살 수 있음

---

# 28. Behavior Tree 역할

Behavior Tree는 행동 선택보다 **선택된 행동의 수행 과정**을 담당한다.

예: Hunt

```text
FindMonster
↓
CanReachTarget?
├─ NO → FindAnotherTarget
└─ YES
   ↓
 MoveToTarget
   ↓
 Attack
   ↓
 MonsterDead?
   ↓
 Loot
   ↓
 EvaluateNextAction
```

PK 예:

```text
Select Target
↓
Evaluate Target
↓
Approach
↓
Attack
↓
Target Dead?
├─ NO → Continue / Flee
└─ YES
   ↓
 Loot
   ↓
 Weight Update
```

---

# 29. 이벤트 기반 Weight 계산

모든 NPC가 매 프레임 모든 Weight를 계산하지 않는다.

```text
NPC Spawn
→ 초기 Weight 계산

Monster 발견
→ Combat 관련 Weight 변경

NPC 발견
→ Trade / PK 관련 Weight 변경

Item 발견
→ Loot 변경

HP 감소
→ Flee / Rest 변경

사건 목격
→ Social / Combat Weight 변경

행동 종료
→ Weighted Random
→ 다음 행동
```

---

# 30. Dirty Flag

```cpp
enum class EWeightDirtyFlag
{
    None        = 0,
    Combat      = 1 << 0,
    Trade       = 1 << 1,
    Loot        = 1 << 2,
    Survival    = 1 << 3,
    Social      = 1 << 4
};
```

예:

```cpp
DirtyFlags |= Combat;
```

필요한 카테고리만 다시 계산한다.

---

# 31. 플레이어가 NPC를 공격했을 때

평화적인 NPC:

```text
Player Attack
↓
Trade 40 → 0
Flee  10 → 80
Fight  5 → 20
```

공격적인 NPC:

```text
Player Attack
↓
Fight 20 → 85
Flee  10 → 5
```

주변 NPC도 사건을 인식할 수 있다.

```text
Player → NPC A 공격
        ↓
   주변 NPC Witness
   ├─ 친구 → Fight Player
   ├─ 겁이 많음 → Flee
   ├─ 탐욕적 → Loot 대기
   └─ 경비 성향 → 신고 / 개입
```

---

# 32. NPC ↔ NPC PK

Dynamic NPC는 다른 Dynamic NPC를 공격할 수 있다.

```cpp
PKWeight =
    BasePK
    + Aggression
    + Greed
    + TargetLootValue
    + PersonalHostility
    - Morality
    - Danger
    - LawRisk;
```

NPC 사망 시:

```text
NPC Death
↓
Inventory Drop
↓
Relationship Update
↓
Witness Update
↓
World Event
↓
Population Update
```

---

# 33. AI LOD

대량의 NPC를 처리하기 위해 AI 시뮬레이션을 단계별로 단순화한다.

```text
LOD 0
플레이어 주변
→ Full Simulation

LOD 1
중거리
→ Simplified Simulation

LOD 2
먼 지역
→ Statistical World Simulation
```

---

# 34. AI LOD 0 - Full Simulation

플레이어가 직접 볼 수 있는 영역.

```text
Behavior Tree        O
Pathfinding          O
Combat               O
Projectile           O
Collision            O
Animation            O
Perception           O
Weighted AI          O
Fuzzy Evaluation     O
```

NPC가 실제로 이동하고 공격한다.

---

# 35. AI LOD 1 - Simplified Simulation

중거리 NPC.

```text
Behavior Tree        Simplified
Pathfinding          Simplified
Combat               Statistical
Projectile           X
Detailed Animation   X
Detailed Collision   X
Weight                O
```

실제 투사체 대신 전투 결과를 계산한다.

예:

```text
NPC AttackPower = 50
MonsterPower    = 30

↓
승률 계산
↓
Random
↓
결과
```

성공 시:

```text
MonsterPopulation -1
NPC EXP           +X
RegionThreat      +1
NPC HP            -Y
```

---

# 36. AI LOD 2 - World Simulation

아주 먼 NPC.

실제 Actor를 만들지 않는다.

```yaml
NPC_ID: 301
Region: NorthernVillage
Level: 13
HP: 85
State: Hunting

Personality:
  Aggression: 0.32
  Greed: 0.71
  Courage: 0.55

ActionWeights:
  Hunt: 55
  Trade: 20
  PK: 5
  Loot: 15
  Rest: 5
```

예:

```text
30분 경과
↓
HuntingEfficiency = 0.72
ExpectedKills = 5
↓
Random Variation
↓
Result = 4 kills
```

결과만 반영:

```text
Region MonsterPopulation -4
Region Threat            +4
NPC EXP                  +EXP
NPC Inventory            +GeneratedLoot
```

---

# 37. LOD 전환

거리 기반 예:

```text
0 ~ 100m
→ LOD 0

100 ~ 500m
→ LOD 1

500m+
→ LOD 2
```

청크 기반 예:

```text
Player Chunk / Adjacent Chunk
→ LOD 0

2~4 Chunk Away
→ LOD 1

Far Chunk
→ LOD 2
```

실제 오픈월드에서는 청크 기반과 거리 기반을 함께 사용하는 것을 권장한다.

---

# 38. 마을 중심 월드 구조

```text
                  강한 몬스터 지역
                         │
          ┌──────────────┴──────────────┐
          │                             │
     몬스터 출몰지역               몬스터 출몰지역
          │                             │
          └──────────────┬──────────────┘
                         │
                    [ 마을 ]
                  상점 / 거래
                  휴식 / 정비
                  Quest NPC
                  Dynamic NPC
```

Dynamic NPC의 기본 생활 루프:

```text
마을
↓
사냥터 이동
↓
몬스터 사냥
↓
아이템 획득
↓
마을 귀환
↓
판매 / 거래 / 장비 교체
↓
다시 사냥
```

---

# 39. 몬스터 지속 출몰

지역별로 몬스터 개체군을 관리한다.

```cpp
struct MonsterZone
{
    int CurrentPopulation;
    int TargetPopulation;
    float SpawnRate;

    int TotalKills;
    int BossThreshold;

    bool BossAlive;
};
```

예:

```text
검은 숲

Target Population : 30
Current Population: 24
Respawn Interval  : 15 sec

Boss Threshold    : 100
Current Score     : 73
```

몬스터가 죽자마자 즉시 리스폰하는 대신 일정 시간에 걸쳐 목표 개체수로 복구한다.

---

# 40. Region Threat

단순 처치 수보다 확장성이 높은 방식.

```cpp
float RegionThreat;
```

예:

```text
일반 몬스터 처치   +1
엘리트 처치        +5
몬스터 거점 파괴   +10
특정 유물 획득     +15
```

```text
RegionThreat >= BossThreshold
→ Boss Spawn Event
```

---

# 41. 플레이어와 NPC가 함께 보스 출현 조건을 만든다

```text
Player Kill
      \
       → RegionThreat ↑ → Boss Spawn
      /
NPC Kill
```

예:

```text
Player Kills = 32
NPC Kills    = 68
Total        = 100

→ Boss Spawn
```

즉 플레이어가 아무것도 하지 않은 지역에서도 NPC의 사냥 때문에 보스가 출현할 수 있다.

---

# 42. NPC의 보스 사냥

보스가 출현하면 NPC도 반응한다.

```text
Boss Spawn
↓
Nearby NPC Detect
↓
Weight Update

HuntBoss = 70
Flee     = 20
Ignore   = 10
```

용감하고 강한 NPC는 보스에 도전할 수 있다.

---

# 43. NPC도 보스를 처치할 수 있음

가능한 결과:

```text
Boss
├─ Player Kill
├─ NPC Kill
└─ Player + NPC Cooperative Kill
```

NPC가 보스를 처치하면:

- EXP 획득
- 보스 Loot 획득
- 장비 착용 가능
- 지역에서 유명해질 수 있음
- WorldEvent에 기록됨

플레이어가 나중에 해당 NPC를 만나면 실제로 보스 장비를 착용하고 있을 수 있다.

---

# 44. LOD 2 상태에서 보스 출현

플레이어가 먼 지역에 있어도 NPC 사냥 결과는 계산된다.

```text
RegionThreat 94
↓
NPC 사냥 결과 +10
↓
RegionThreat 104
↓
BossSpawned = true
```

실제 Actor는 아직 생성하지 않는다.

```yaml
BossWorldState:
  BossAlive: true
  BossType: ForestGuardian
  BossHP: 100
```

플레이어가 접근하면:

```text
Chunk Load
↓
Boss Actor Spawn
↓
저장된 상태 적용
```

---

# 45. LOD 2 상태에서 NPC vs Boss

통계 계산:

```text
BossPower
VS
NPC GroupPower
```

예:

```text
BossPower = 1000

NPC A = 300
NPC B = 250
NPC C = 400

GroupPower = 950
```

결과 예:

```text
BossAlive = false

NPC A HP 감소
NPC B 사망
NPC C HP 감소

BossLoot → NPC C
```

---

# 46. World Event System

플레이어가 없어도 일어난 사건을 기록한다.

```cpp
struct WorldEvent
{
    EventID ID;
    EventType Type;
    RegionID Region;
    GameTime Time;
    EntityID Instigator;
    EntityID Target;
    EventResult Result;
};
```

예:

```yaml
Event:
  Type: BossKilled
  Region: BlackForest
  Instigator: NPC_371
  Target: Boss_12
  Time: Day_37_14_32
```

NPC 대화:

```text
"검은 숲의 괴물이 죽었다더군."
"누가 잡았는데?"
"카일이라는 사냥꾼이라던데."
```

실제 `NPC_371`이 카일이다.

---

# 47. Quest NPC

Quest NPC는 Dynamic NPC와 별도로 관리한다.

```text
Quest NPC
├─ Main Quest NPC
└─ Sub Quest NPC
```

목적:

- 스토리 진행 보장
- 퀘스트 제공
- 플레이어에게 세계관 정보 제공

기본적으로 다음 기능은 제한한다.

```text
랜덤 PK 참여       X
약탈              X
무작위 장거리 이동 X
영구 사망          제한
```

---

# 48. Main Quest

Main Quest는 세계의 핵심 미스터리를 추적한다.

예:

```text
몬스터 출현 조사
↓
존재하지 않았던 폐허
↓
첫 보스
↓
첫 죽음 / 부활
↓
계승자 발견
↓
과거의 기록
↓
몬스터와 죽음의 관계
↓
세계 변화의 원인
↓
최종 미스터리
```

초반에는 단순한 몬스터 사냥 퀘스트처럼 시작하고, 점점 세계의 법칙을 조사하게 만든다.

---

# 49. Sub Quest

서브 퀘스트 NPC 예:

```text
대장장이
→ 광석 / 몬스터 재료

약초사
→ 특정 지역 탐험 / 채집

사냥꾼
→ 몬스터 토벌

상인
→ 물건 회수 / 운송

경비대장
→ 몬스터 / 도적 토벌

주민
→ 실종자 / 물건 찾기
```

---

# 50. Quest System

NPC 안에 퀘스트 전체 로직을 하드코딩하지 않는다.

```text
Quest NPC
↓
Quest ID
↓
Quest Manager
↓
Quest Data
```

```cpp
enum class EQuestType
{
    Main,
    Sub
};

enum class EQuestState
{
    Locked,
    Available,
    Active,
    Completed,
    Failed
};
```

```cpp
struct QuestData
{
    int QuestID;
    EQuestType Type;
    EQuestState State;

    int QuestGiverID;
    int RequiredLevel;

    std::vector<QuestObjective> Objectives;

    QuestReward Reward;

    int NextQuestID;
};
```

---

# 51. Quest Objective

```cpp
enum class EObjectiveType
{
    Kill,
    Collect,
    Talk,
    Explore,
    BossKill,
    Deliver,
    Investigate
};
```

예:

```text
Kill
→ 몬스터 10마리 처치

Collect
→ 잔흔 5개 수집

Talk
→ NPC와 대화

Explore
→ 폐허 발견

BossKill
→ 지역 보스 처치

Deliver
→ 아이템 전달

Investigate
→ 특정 오브젝트 조사
```

---

# 52. Quest와 World Event 연결

같은 사건이 월드 시스템과 퀘스트에 동시에 반영된다.

```text
Player kills Monster
        ↓
  MonsterKilled Event
     /            \
    ↓              ↓
World System   Quest Manager
Threat +1      KillCount +1
```

퀘스트용 몬스터와 월드 몬스터를 따로 만들 필요가 없다.

---

# 53. NPC가 먼저 퀘스트 보스를 잡은 경우

게임 세계가 플레이어를 기다리지 않도록 한다.

```text
NPC kills Boss
↓
BossKilled WorldEvent
↓
Quest Update
```

퀘스트가 단순 실패하는 대신 상황이 변경될 수 있다.

예:

```text
기존 목표:
"검은 숲의 보스를 처치하라."

NPC가 먼저 처치

↓ 변경

"검은 숲의 보스가 사망했다.
시체 또는 전투 흔적을 조사하라."
```

---

# 54. Quest NPC 생활 연출

Quest NPC도 같은 자리에 영구 정지시키지 않는다.

예:

```text
07:00 집에서 나옴
08:00 대장간 이동
08:00~18:00 작업
18:00 술집 이동
21:00 집 이동
22:00 취침
```

단순한 Schedule 기반 BT로 처리한다.

---

# 55. Dynamic NPC Population

Quest NPC를 제외한 Dynamic NPC는 게임 시작 시 대량 생성한다.

예:

```cpp
DynamicNPCPopulation = 500;
TargetPopulation     = 500;
```

동작:

```text
게임 시작
500 / 500

NPC 1명 사망
↓
499 / 500

새로운 NPC 생성
↓
500 / 500
```

핵심:

> 일반 Dynamic NPC의 수가 무한히 증가하지 않도록 목표 인구수를 유지한다.

---

# 56. 신규 NPC 생성 시점

NPC가 사망했다고 그 자리에서 즉시 새 NPC를 생성하지 않는다.

```text
NPC Death
↓
Population Manager에 빈자리 등록
↓
Spawn Delay
↓
적절한 지역 선택
↓
NPC Data 생성
↓
월드에 등록
↓
필요 시 Actor 생성
```

새 NPC는 마을, 정착지, 먼 지역 등 자연스러운 곳에서 등장한다.

---

# 57. Region Population

월드 전체 인구만 고정하면 특정 지역으로 과도하게 몰릴 수 있으므로 지역별 인구 통계를 둔다.

예:

```text
World Dynamic Population = 500

시작 마을 권역    80
검은 숲 권역      60
북부 마을 권역    90
폐허 권역         40
도시 권역        150
기타 지역         80
```

그러나 지역별 수치는 절대 고정하지 않는다.

예:

```text
북부 지역에 강한 Boss 등장
↓
NPC Death 증가
↓
Danger 증가
↓
Flee Weight 증가
↓
NPC 이주

90 → 72 → 55
```

반대로 안전하고 거래가 활발한 지역은 NPC가 유입될 수 있다.

---

# 58. NPC Population Manager

```cpp
class NPCPopulationManager
{
public:
    int TargetPopulation;
    int CurrentPopulation;

    void InitializePopulation();

    void OnNPCDeath(NPC_ID DeadNPC);

    void RequestNewNPC();

    RegionID SelectSpawnRegion();

    NPCData GenerateNPC();

    void RegisterNPC(NPCData NewNPC);
};
```

---

# 59. 신규 Dynamic NPC Personality 생성

완전 균등 랜덤보다는 Archetype + 랜덤 변동을 권장한다.

예:

```text
50% 일반 성향
20% 사냥 성향
15% 거래 성향
10% 탐험 성향
 5% 공격 / 약탈 성향
```

그 안에서 개별 Personality를 무작위 변동한다.

예:

```cpp
NewNPC.Aggression  = Random(...);
NewNPC.Greed       = Random(...);
NewNPC.Courage     = Random(...);
NewNPC.Sociability = Random(...);
NewNPC.Morality    = Random(...);
NewNPC.Curiosity   = Random(...);
```

---

# 60. Successor NPC와 Population 충돌 문제

기존 핵심 규칙:

```text
Player Death
→ 반드시 Successor NPC 1명 생성
```

새 인구 규칙:

```text
Dynamic NPC
→ 목표 인구 유지
```

두 시스템이 충돌할 수 있다.

예:

```text
500 Dynamic NPC
↓
Player Death
↓
Successor NPC +1
↓
501
```

## 권장 해결안

```text
Quest NPC
→ 별도 고정

Normal Dynamic NPC
→ 목표 인구 관리

Successor NPC
→ 반드시 생성되는 특수 Dynamic NPC

장기적으로:
Successor NPC가 증가하면
일반 Dynamic NPC 신규 생성량을 줄여
전체 인구가 목표 범위에 다시 수렴
```

즉 플레이어의 죽음으로 생성되는 NPC를 제거하거나 지연시키지 않고, 일반 NPC 측에서 인구 균형을 조절한다.

---

# 61. NPC 사망 데이터

NPC는 죽으면 단순 삭제되지 않고 기록을 남긴다.

```yaml
NPCDeath:
  NPC_ID: NPC_103
  Alive: false
  CauseOfDeath: Player
  DeathLocation: BlackForest
  DeathTime: Day_15
```

가능한 영향:

- Inventory Drop
- 관계 변화
- 목격자 기억
- 지역 위험도
- Reputation
- WorldEvent
- Population 빈자리

---

# 62. 전체 NPC와 월드 시뮬레이션 구조

```text
                    WORLD
                      │
            ┌─────────┴─────────┐
            │                   │
         Village              Region
            │                   │
       Quest NPC          Monster Spawn
       Dynamic NPC             │
            │                   │
            └─────────┬─────────┘
                      ↓
               Player / NPC Hunt
                      ↓
                 Monster Kill
                      ↓
              Region Threat ↑
                      ↓
               Boss Threshold
                      ↓
                  Boss Spawn
                      ↓
         ┌────────────┴────────────┐
         │                         │
      Player                     NPC
      Combat                     Combat
         │                         │
         └────────────┬────────────┘
                      ↓
                  Boss Death
                      ↓
                  Loot Owner
                      ↓
                 World Event
                      ↓
               Persistent Save
```

---

# 63. Player vs NPC 사건 흐름

```text
Player
↓
중립 NPC 좌클릭
↓
Combat Target 지정
↓
Auto Aim
↓
NPC 공격
↓
NPC Weight 변화
↓
주변 NPC Witness
↓
각자 Personality에 따라 반응

├─ Fight
├─ Flee
├─ Loot
├─ 신고
└─ 무시
```

---

# 64. 게임의 3가지 콘텐츠 계층

## 64.1 Fixed Main Story

개발자가 직접 설계하는 핵심 이야기.

- 세계의 죽음
- 부활
- 계승
- 몬스터
- 범람
- 핵심 미스터리

## 64.2 Sub Quest / Regional Story

마을과 지역 중심의 작은 이야기.

- 사냥
- 실종
- 재료
- 지역 갈등
- 몬스터 토벌
- 거래
- 탐사

## 64.3 Dynamic World Event

NPC AI와 월드 시스템 때문에 자연스럽게 생성되는 사건.

예:

- NPC가 Boss를 먼저 처치
- 유명 사냥꾼 NPC 사망
- PK NPC가 상인 공격
- 강한 NPC가 Boss 장비 획득
- 특정 지역 NPC 대량 이주
- 몬스터 증가로 마을 주변 위험 상승
- NPC끼리 반복된 갈등으로 적대 관계 형성

---

# 65. Story Flag 예시

```yaml
StoryFlags:
  FirstMonsterKilled: false
  FirstItemObtained: false
  FirstLevelUp: false

  FirstDungeonDiscovered: false
  FirstBossKilled: false

  FirstDeathOccurred: false
  FirstResurrectionOccurred: false

  FirstSuccessorCreated: false
  FirstSuccessorMet: false

  MonsterCanSpeakDiscovered: false

  AutoAimTruthDiscovered: false
  MonsterOriginDiscovered: false
  WorldExpansionDiscovered: false

  FinalBossDefeated: false
```

---

# 66. 개발 데이터 구조 예시

```text
GameData/
│
├── Story/
│   ├── MainStory
│   ├── Chapters
│   ├── StoryFlags
│   └── Lore
│
├── Quest/
│   ├── MainQuest
│   ├── SubQuest
│   ├── QuestObjectives
│   └── QuestRewards
│
├── Player/
│   ├── PlayerStats
│   ├── PlayerTraits
│   └── PlayerHistory
│
├── Combat/
│   ├── Targeting
│   ├── Projectile
│   └── Damage
│
├── Monster/
│   ├── MonsterData
│   ├── BossData
│   ├── SpawnZone
│   └── DropTables
│
├── NPC/
│   ├── NPCData
│   ├── DynamicNPC
│   ├── QuestNPC
│   ├── SuccessorNPC
│   ├── Personality
│   ├── ActionWeights
│   ├── Relationship
│   └── Memory
│
├── AI/
│   ├── FuzzyRules
│   ├── WeightSystem
│   ├── BehaviorTrees
│   └── AILOD
│
├── World/
│   ├── ChunkData
│   ├── RegionData
│   ├── WorldEvents
│   ├── RegionThreat
│   └── GeneratedLocations
│
├── Population/
│   ├── NPCPopulation
│   └── MonsterPopulation
│
└── Death/
    ├── DeathRecord
    ├── TraitTransfer
    └── Resurrection
```

---

# 67. 주요 시스템 클래스 관계 예시

```text
GameMode / WorldSubsystem
│
├─ QuestManager
│
├─ WorldEventManager
│
├─ NPCPopulationManager
│
├─ MonsterPopulationManager
│
├─ RegionThreatManager
│
├─ AILODManager
│
└─ SaveManager
```

NPC:

```text
NPCData
├─ Personality
├─ ActionWeights
├─ Relationship
├─ Memory
├─ Inventory
├─ CurrentState
└─ Region
```

Dynamic NPC:

```text
NPCData
↓
Fuzzy Evaluator
↓
Weight System
↓
Weighted Random
↓
Action
↓
Behavior Tree / LOD Simulation
```

---

# 68. Prototype 개발 우선순위

## Prototype 01 - 기본 전투

```text
Player Movement
↓
Enemy Spawn
↓
Auto Aim
↓
Projectile
↓
Enemy Death
↓
EXP
↓
Level Up
↓
Item Drop
```

## Prototype 02 - NPC 전투

```text
Dynamic NPC Spawn
↓
NPC Targeting
↓
NPC Hunt
↓
Player Left Click NPC
↓
Auto Aim NPC Attack
↓
NPC Reaction
```

## Prototype 03 - AI 의사결정

```text
Personality
↓
Fuzzy Rule
↓
Weight
↓
Weighted Random
↓
Behavior Tree
```

## Prototype 04 - AI LOD

```text
LOD 0 Full Actor
↓
LOD 1 Simplified
↓
LOD 2 Statistical Simulation
↓
State Restore
```

## Prototype 05 - 월드 생태

```text
Monster Population
↓
NPC Hunt
↓
Region Threat
↓
Boss Spawn
↓
NPC / Player Boss Fight
↓
World Event
```

## Prototype 06 - NPC 인구

```text
Initial Dynamic Population
↓
NPC Death
↓
Vacancy
↓
New NPC Data
↓
Population Restore
```

## Prototype 07 - Quest

```text
Quest NPC
↓
Quest Manager
↓
Objectives
↓
World Event Integration
↓
Main / Sub Quest
```

## Prototype 08 - 죽음과 계승

```text
Player Death
↓
Death Record
↓
Trait Analysis
↓
Successor NPC
↓
Resurrection
↓
Persistent Save
```

---

# 69. 최종 게임 구조 요약

```text
Combat RPG
+
Auto Aim
+
Monster Farming
+
Level Growth
+
Open World Exploration
+
Dynamic NPC Simulation
+
Fuzzy / Weight AI
+
Behavior Tree
+
AI LOD
+
NPC Hunting / Trading / Looting / PK
+
Player vs NPC Combat
+
Monster Population
+
Region Threat
+
Dynamic Boss Spawn
+
Main Quest / Sub Quest
+
Dynamic World Event
+
NPC Population
+
Death / Resurrection
+
Successor NPC
+
Persistent World
+
Mystery Story
```

---

# 70. 핵심 디자인 철학

## 70.1 플레이어가 없어도 세계가 움직인다

NPC는 플레이어를 기다리지 않는다.

NPC는:

- 몬스터를 사냥한다.
- 레벨이 오를 수 있다.
- 장비를 얻는다.
- 거래한다.
- 약탈한다.
- 다른 NPC와 싸운다.
- 플레이어를 공격할 수 있다.
- 보스를 출현시킨다.
- 보스를 죽일 수 있다.
- 다른 지역으로 이동할 수 있다.
- 죽을 수 있다.

---

## 70.2 NPC는 완전 랜덤이 아니다

```text
Personality
+
상태
+
환경
+
경험
↓
Weight
↓
Weighted Random
```

따라서 행동에는 확률성이 있지만, NPC의 성격과 과거 경험이 행동에 계속 영향을 준다.

---

## 70.3 세계는 결과를 기억한다

중요 사건은 WorldEvent로 저장한다.

```text
Boss Spawn
Boss Kill
NPC Death
Player PK
지역 변화
NPC 이주
아이템 획득
Quest 사건
```

플레이어가 해당 장소에 없었더라도 사건은 역사로 남는다.

---

## 70.4 성능을 위해 시뮬레이션 해상도를 바꾼다

```text
가까움
→ 정확한 Actor / BT / Combat

중거리
→ 단순 계산

먼 거리
→ 통계 시뮬레이션
```

모든 NPC를 동일한 비용으로 계산하지 않는다.

---

## 70.5 Quest와 Simulation을 분리하되 서로 연결한다

```text
Quest Story
↕
World Event
↕
Dynamic Simulation
```

퀘스트의 진행 안정성은 유지하면서도, NPC가 먼저 보스를 잡는 등의 월드 변화가 퀘스트 상황에 영향을 줄 수 있다.

---

# 71. 현재 추가로 결정해야 할 항목

아래 내용은 앞으로 확정이 필요한 부분이다.

- Dynamic NPC 목표 인구 수
- 지역별 최소 / 최대 권장 인구
- Successor NPC 증가 시 인구 수렴 방식
- NPC 영구 사망 여부
- Quest NPC 공격 가능 여부
- Main Quest NPC 무적 여부
- Player PK의 Reputation / 범죄 시스템
- 마을 경비 시스템
- NPC끼리 파티를 구성할 수 있는지
- NPC 레벨 제한
- NPC 장비 교체 로직
- Boss Respawn 여부
- Region Threat 감소 방식
- Boss 출현 후 Threat 초기화 방식
- 보스가 NPC에 의해 죽었을 때 Quest 처리 규칙
- Loot 소유권
- NPC가 플레이어 아이템을 약탈할 수 있는지
- 플레이어 사망 후 아이템 패널티
- 부활 위치
- 몬스터와 계승자의 실제 관계
- 범람의 정확한 원인
- 세계 확장의 정확한 법칙
- 최종 메인 스토리의 결말

---

# 72. 한 문장 정리

> **플레이어와 NPC가 동일한 세계 안에서 사냥하고 성장하고 거래하고 약탈하고 죽이며, 그 결과가 월드와 다른 NPC의 행동에 계속 영향을 주고, 플레이어가 없어도 세계가 스스로 움직이는 오픈월드 RPG.**

그리고 스토리 측 핵심 문장은 다음과 같이 유지할 수 있다.

> **「죽음은 사라지는 것이 아니라, 세계에 무언가를 남기는 사건이다.」**
