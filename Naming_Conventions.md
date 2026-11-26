# 이름 규칙 (Naming Conventions)

작성: 2026-11-26. 코드·데이터·태그 이름 규칙을 한곳에 모은 문서.
에셋(파일) 이름과 폴더 규칙은 `Asset_Naming_Guide.md`, 스킬 표 작성 규칙은 `Skill_Spec.md` 0장을 함께 본다.

표기: [확정] 지금 코드와 데이터가 따르는 규칙 / [제안] 아직 적용 전 / [정리 필요] 현재 어긋나 있는 곳

---

## 1. C++ 클래스

| 종류 | 형식 | 예 |
|---|---|---|
| 액터 | `A` + 이름 | `ANonCharacterBase`, `ABossCharacter`, `ADamageAOE` |
| UObject·컴포넌트 | `U` + 이름 | `UNonGuardComponent`, `USkillManagerComponent` |
| 구조체 | `F` + 이름 | `FSkillRow`, `FSkillSpecEntry` |
| 열거형 | `E` + 이름 | `EWeaponType`, `EJobClass` |
| 인터페이스 | `I` + 이름 | `INonInteractableInterface` |
| 게임플레이 어빌리티 | `UGA_` + 이름 | `UGA_HitReaction`, `UGA_Boss_Grab` |
| 게임플레이 이펙트 (C++) | `UGE_` + 이름 | `UGE_SkillCooldown`, `UGE_SkillCost_SP` |
| 애님 노티파이 / 노티파이 스테이트 | `UAnimNotify_` / `UANS_` + 이름 | `UAnimNotify_SwapWeaponSocket`, `UANS_DodgeIFrame` |
| BT 태스크 / 서비스 | `UBTTask_` / `UBTService_` + 이름 | `UBTTask_EnemySkill`, `UBTService_UpdateTarget` |
| 프로젝트 공용 기반 클래스 | `Non` 접두어 | `ANonCharacterBase`, `ANonPlayerController`, `UNonAttributeSet` |

* C++ 클래스와 같은 이름의 BP를 만들지 않는다. BP는 `BP_`, `GA_`(BP 어빌리티) 등 에셋 접두어를 따르되 C++ 이름과 겹치지 않게 한다 (`Asset_Naming_Guide.md`).

## 2. 열거형 [확정]

* 이름: `E` + PascalCase. 값: PascalCase, 열거형 이름을 값에 반복하지 않는다 (`EWeaponType::Sword`, `EWeaponType_Sword` 아님).
* 비어 있음을 뜻하는 값은 `None`.
* 무기 이름은 아래 표를 모든 곳(열거형, 태그, 에셋 폴더, 아이템 ID)에서 똑같이 쓴다.

| 무기 | EWeaponType | EWeaponStance | 태그 | 에셋 폴더 |
|---|---|---|---|---|
| 한손검 | `Sword` | `SwordShield` (방패와 함께) | `Weapon.Type.Sword` | `Sword` |
| 양손 대검 | `GreatSword` | `GreatSword` | `Weapon.Type.GreatSword` | `GreatSword` |
| 방패 | `Shield` | (한손검 자세에 포함) | `Weapon.Type.Shield` | `Shield` |
| 지팡이 | `Staff` | `Staff` | `Weapon.Type.Staff` | `Staff` |
| 맨손 | | `Unarmed` | | |

### 열거형 통합 기록 (2026-11-26 정리 완료)

| 이전 | 지금 | 비고 |
|---|---|---|
| `EAOEShape`(DamageAOE.h) + `EAOEConfigShape`(SkillTypes.h, 값 순서 다름) | `EAOEShape` 하나 (Combat/CombatTypes.h) | 표시 이름 깨짐("캡슐형")도 수정 |
| `EHitTeamSide`(ANS_HitTrace.h) + `ETeamSideAOE`(DamageAOE.h) | `ETeamSide` 하나 (Combat/CombatTypes.h) | |

* 옛 이름으로 저장된 에셋 값은 `DefaultEngine.ini`의 `[CoreRedirects]`가 새 열거형으로 읽어 준다. 에셋을 다시 저장한 뒤에도 남겨 둬도 무해.
* 여러 파일에서 쓰는 공용 열거형은 `Combat/CombatTypes.h`처럼 가벼운 헤더에 두고, 같은 뜻의 열거형을 새로 만들지 않는다.

## 3. 게임플레이 태그 [확정]

* 형식: `최상위.분류.세부`, 각 단계 PascalCase. 예: `State.Grabbed`, `Effect.Hit.KnockDown`, `Cooldown.Skill.Ber_A_Crush`
* C++에서는 문자열로 찾지 않고 `NonTags::`(Core/NonGameplayTags.h)의 네이티브 태그를 쓴다. 이름은 점을 밑줄로 바꾼 형태: `State.Grabbed` → `NonTags::State_Grabbed`
* 태그 이름의 대소문자만 바꾸는 이름 변경은 하지 않는다 (엔진이 대소문자를 구분하지 않아 리다이렉트가 오히려 태그를 깨뜨림).

| 최상위 | 용도 |
|---|---|
| `Ability.` | 어빌리티 식별 (발동·취소·차단 대상 지정) |
| `State.` | 캐릭터 상태 (잡힘, 넉다운, 그로기, 공격 중 등) |
| `Effect.` | 피격 종류 등 이펙트 분류 (`Effect.Hit.*`) |
| `Event.` | 게임플레이 이벤트 (`Event.Grabbed`, `Event.Parry.Success`) |
| `Cooldown.` | 쿨타임. 플레이어 스킬은 `Cooldown.Skill.<스킬 ID>` |
| `Data.` | SetByCaller 등 값 전달용 키 |
| `Combat.` | 전투 판정 상태 (`Combat.Casting`, `Combat.BlockHit`) |
| `Weapon.` | 무기 종류 (`Weapon.Type.*`) |
| `Damage.` | 피해 속성 (`Damage.Unblockable`) |
| `Enemy.`, `Boss.` | 적·보스 스킬 분류 |
| `Movement.` | 이동 제한 |
| `GameplayCue.` | 게임플레이 큐 |

## 4. 데이터 ID

### 스킬 ID [확정]

`직업코드3자_종류_이름` — 자세한 규칙은 `Skill_Spec.md` 0장.

| 직업 | 코드 | EJobClass |
|---|---|---|
| 디펜더 | `Def` | `Defender` |
| 버서커 | `Ber` | `Berserker` |
| 어쌔신 | `Asn` | (미구현) |
| 소서러 | `Sor` | `Sorcerer` |
| 아처 | `Arc` | (미구현) |
| 클레릭 | `Clr` | `Cleric` |
| 공용 | `Com` | |

종류: `A` 액티브(궁극기 포함) / `P` 패시브 / `CB` 긴급 회피기. 예: `Ber_A_Crush`, `Ber_P_RageMastery`, `Def_CB_ShieldBastion`. 한 번 정한 ID는 바꾸지 않는다 (세이브, 퀵슬롯, 쿨타임 태그가 ID로 연결됨).

### 아이템 ID [확정, 2026-11-26 데이터 적용]

`분류_세부_변형`, 각 부분 PascalCase. 앞에서부터 큰 분류 → 작은 분류 순서라서 정렬하면 같은 종류끼리 모인다.

| 분류 | 형식 | 예 |
|---|---|---|
| 무기 | `무기타입_재질` (무기타입은 2장 표의 EWeaponType 이름) | `Sword_Iron`, `GreatSword_Bronze`, `Shield_Wood`, `Staff_Wood` |
| 방어구 | `부위_재질` (부위는 EEquipmentSlot 이름: Head / Chest / Legs / Hands / Feet) | `Head_Iron`, `Chest_Leather`, `Hands_Leather`, `Feet_Plate` |
| 장신구 | `Necklace_` / `Earring_` / `Ring_` + 이름 | `Ring_Ruby` |
| 소모품 | `종류_효과_크기` | `Potion_HP_Small`, `Potion_MP_Large`, `Scroll_Return` |
| 재료 | `Mat_이름` / 보석은 `Gem_색_크기` | `Mat_IronOre`, `Gem_Red_Small` |
| 퀘스트·기타 | `Quest_이름` / `Key_이름` | `Quest_OldLetter`, `Key_Dungeon01` |

* 재질 단어: 금속 `Bronze` < `Iron` < `Steel`..., 방어구 재질 `Plate` / `Chain` / `Leather` / `Robe`, 나무 `Wood`.
* 같은 종류·재질에 여러 개가 필요하면 고유 이름을 붙인다 (`Sword_Iron_Knight`). 뜻 없는 번호(`Potion1`)는 쓰지 않는다. 번호는 순서 자체가 뜻일 때만 (`Key_Dungeon01`).
* 등급(Rarity)은 ID에 넣지 않는다 (데이터 컬럼). 강화 수치, 랜덤 옵션도 ID가 아니라 아이템 인스턴스 데이터.
* 한 번 정한 ID는 바꾸지 않는다. 세이브(인벤토리·장비·퀵슬롯), 드롭 테이블, 상점 목록이 참조한다.
* 대소문자만 다른 두 ID를 만들지 않는다. FName은 대소문자를 구분하지 않아 `Greatsword_Bronze`와 `GreatSword_Bronze`를 같은 ID로 본다.

#### ID를 바꿔야 할 때

1. `Source/Non/Public/Data/ItemIdRedirects.h`의 표에 `옛 ID -> 새 ID` 한 줄 추가 (지우지 않음). 옛 세이브는 불러올 때(UNonSaveGame::Serialize) 자동으로 새 ID로 바뀐다.
2. DT_ItemData 행 이름 변경 (대소문자만 바꿀 때는 임시 이름을 한 번 거친다).
3. 참조 갱신: DT_EnemyData 드롭 목록, BP_BaseNPC 상점 목록(ShopItems), 줍기 아이템(BP_PickupItem과 맵에 배치된 것), 채팅 테스트 명령(`/item`, ChatBoxWidget.cpp).

#### 변경 기록

| 날짜 | 옛 ID | 새 ID |
|---|---|---|
| 2026-11-26 | `Greatsword_Bronze` | `GreatSword_Bronze` |
| 2026-11-26 | `WoodShield` | `Shield_Wood` |
| 2026-11-26 | `Glove` | `Hands_Leather` |
| 2026-11-26 | `Staff` | `Staff_Wood` |
| 2026-11-26 | `Potion` (초록 아이콘, 몬스터 드롭) | `Potion_HP_Small` |
| 2026-11-26 | `Potion1` (빨강 아이콘, 상점) | `Potion_HP_Medium` |
| 2026-11-26 | `Helm` | `Head_Iron` |

`Sword_Iron`은 규칙과 같아서 그대로. `Potion_HP_Small`과 `Potion_HP_Medium`은 지금 효과 수치가 같음 (둘 다 기본 30% 회복) — 회복량을 나눌 때 데이터만 고치면 된다.

## 5. 변수·함수

* UE 기본 규칙: 변수·함수 PascalCase, bool은 `b` 접두어 (`bIsGroggy`), 네트워크 함수는 `Server_`/`Client_`/`Multicast_` 또는 `Server`/`Client`/`Multicast` 접두어.
* [정리 필요] 네트워크 함수 접두어에 밑줄이 있는 것(`Server_DebugBossGroggy`)과 없는 것(`ServerAddItem`)이 섞여 있음. 새로 만드는 것은 밑줄 있는 형식(`Server_`, `Client_`, `Multicast_`)으로 통일하고, 기존 것은 해당 코드를 손볼 때 함께 바꾼다.
