# 에셋 이름·폴더 정리 가이드 (2026-11-24)

대상은 `Content/Non` (프로젝트에서 직접 만든 에셋)만이다. 마켓플레이스 팩(`Fab`, `GreatSword`, `ParagonKwang`, `X` 등)은 원본 이름을 그대로 둔다.
코드·태그 쪽 이름은 이미 통일했다 (`EWeaponType`: Sword / GreatSword / Shield / Staff, `EWeaponStance::GreatSword`, `Weapon.Type.Sword`, `Weapon.Type.GreatSword`).

---

## 1. 규칙

### 이름 형식

`접두어_대상_동작[_세부][_방향][_번호]`

* 대상: 무기는 `Sword`(한손검), `GreatSword`(대검), `Staff`, `Shield`. 무기와 상관없는 것은 `Common`. 적은 `Dummy`, `Golem`.
* 방향: `F` `B` `L` `R` `FL` `FR` `BL` `BR` (지금 쓰는 방식 그대로).
* 번호: 두 자리 `01`, `02` (같은 동작이 여러 개일 때만).
* 단어 첫 글자는 대문자 (`Idle`, `KnockDown`, `GetUp`). 소문자 시작 금지 (`anim_roll_back`, `A_staff_cast_Idle`, `A_Golem_idle` 같은 것).
* 쓰지 않는 약어: `GS`, `1HD`, `2HD`, `2hd`, `atk`, `Anim_`, `Am_`.

### 접두어

| 종류 | 접두어 | 예 |
|---|---|---|
| 애니메이션 시퀀스 | `A_` | `A_GreatSword_Attack01` |
| 애니메이션 몽타주 | `AM_` | `AM_GreatSword_Attack01` |
| 블렌드 스페이스 | `BS_` | `BS_GreatSword_Move_8Way` |
| 애니메이션 BP | `ABP_` | `ABP_Non` |
| 애님 노티파이 BP | `AN_` / `ANS_` | `AN_SpawnDecal` |
| 스켈레탈 메시 | `SKM_` | `SKM_Sword` |
| 스켈레톤 | `SK_` | `SK_SwordDummy` |
| 스태틱 메시 | `SM_` | `SM_StaffDummy` |
| 피직스 에셋 | `PA_` | `PA_Golem` |
| 머티리얼 / 인스턴스 / 함수 | `M_` / `MI_` / `MF_` | `MI_Sword` |
| 텍스처 | `T_` | `T_Sword_BC` |
| 나이아가라 | `NS_` | `NS_Stun` |
| 사운드 웨이브 / 큐 / 메타사운드 | `SW_` / `SC_` / `MS_` | `SW_Sword_Attack01` |
| 블루프린트 | `BP_` | `BP_Boss_Golem` |
| 어빌리티 / 이펙트 / 큐 | `GA_` / `GE_` / `GC_` | `GA_GreatSword_Combo1` |
| 데이터 테이블 / 데이터 에셋 / 커브 테이블 | `DT_` / `DA_` / `CT_` | `DT_ItemData` |
| 위젯 | `WBP_` | `WBP_Inventory` |
| 레벨 | `LV_` | `LV_Town1` |
| BT / 블랙보드 | `BT_` / `BB_` | |
| 열거형(BP) | `E_` | `E_InvFilter` |

`SKM_`(메시)와 `SK_`(스켈레톤)는 지금 장비 폴더에서 맞게 쓰고 있다. 그대로 유지.

---

## 2. 폴더 구조

### 지금 구조에 대한 판단

큰 틀(종류별 최상위 폴더 `Animation / Blueprints / Characters / Data / Equipment / UI / VFX / Sound`, 마켓 팩은 `Content` 바로 아래로 분리)은 적절하다. 이 크기 프로젝트에서는 이 방식이 찾기 쉽다. 바꿀 필요 없음.

문제는 그 안쪽이다.

| 문제 | 위치 | 왜 문제인지 |
|---|---|---|
| 무기 폴더 이름이 코드와 다름 | `Animation/Player/1HD`, `2HD` | 코드·어빌리티 폴더는 `Sword`, `GreatSword`인데 애니메이션만 `1HD`, `2HD` |
| 공용 애니메이션이 한손검 폴더에 섞임 | `1HD` 바로 아래 사망·점프·기상·기절 | 대검도 쓰는 동작인지 한손검 전용인지 구분이 안 됨 |
| 원본(리타깃 소스)과 실제 사용 에셋이 섞임 | `2HD/Retarget` (176개), `2HD` 바로 아래 `A_hm_2hd_*` | 실제로 쓰는 것은 일부 몽타주뿐. 나머지는 원본 보관용 |
| 데이터 테이블이 여기저기 | `UI/ShopAndDialouge/DT_MerchantDialogue`, `UI/Panels/Inventory/EInvFilter` | 데이터는 `Data`에 모으는 편이 관리가 쉬움 (`DT_RichTextStyle`은 UI 전용이라 UI에 둬도 됨) |
| 오타 폴더 | `VFX/Mateirals`, `UI/ShopAndDialouge` | `Materials`, `ShopAndDialogue` |
| 같은 종류인데 위치가 다름 | `Blueprints/Abilities/GC_LevelUp` | 나머지 큐는 `GameplayCues` 안에 있음 |
| 장비 무기 폴더 불균형 | `Equipment/Weapon` | 한손검·방패만 하위 폴더가 있고 대검·스태프 더미는 `Weapon` 바로 아래 |
| 임시·자동 생성 폴더 | `Maps/_GENERATED`, `Content/_GENERATED`, `Mutable_Test`, `Maps/NewMaterial*` | 모델링 도구·테스트 결과물. 쓰는 것만 옮기고 나머지 삭제 |
| 템플릿 잔재 중복 | `Characters/Player/Rigs`(63개)와 `Characters/Mannequins/Rigs`, `Characters/Player/Animations`(ABP_Manny/Quinn) | 같은 리그가 두 벌. 포즈 에셋 "out-of-date" 경고도 여기서 나옴 |
| 이름 없는 폴더 | `Content/X`, `Content/Fab/NewFolder` | 무엇이 들었는지 이름으로 알 수 없음 (`X` = 무기별 애니메이션 라이브러리) |
| 적 메시 위치 | `Characters/Enemy/SKM_Manny_Enemy` | 보스는 `Boss/Golem` 하위인데 더미는 `Enemy` 바로 아래 |

### 제안 구조 (Animation/Player)

```
Animation/Player/
  ABP_Non
  Common/          무기 공용: 사망, 기상, 점프, 기절, 구르기, 맨손(MM_*)
  Sword/           (1HD)  Attack, Dodge, DrawSheathe, Guard, Hit, Move
  GreatSword/      (2HD)  Attack, Dodge, DrawSheathe, Guard, Hit, Jump, Move, Death
    _Source/       (2HD/Retarget + A_hm_2hd_*) 원본 보관. 이름 안 바꿈
  Staff/           Cast, DrawSheathe, Move
```

### 제안 구조 (기타)

| 지금 | 바꿀 위치 |
|---|---|
| `Equipment/Weapon/*GreatSwordDummy` | `Equipment/Weapon/GreatSword/` |
| `Equipment/Weapon/*StaffDummy` | `Equipment/Weapon/Staff/` |
| `Equipment/Weapon/SM_SwordDummy` | `Equipment/Weapon/Sword/` |
| `Characters/Enemy/SKM_Manny_Enemy` | `Characters/Enemy/Dummy/` |
| `Blueprints/Abilities/GC_LevelUp` | `Blueprints/Abilities/GameplayCues/` |
| `UI/ShopAndDialouge/DT_MerchantDialogue` | `Data/` |
| `UI/Panels/Inventory/EInvFilter` | `Data/` (또는 `UI/Enums/`) |
| `VFX/Mateirals` | `VFX/Materials` |
| `UI/ShopAndDialouge` | `UI/ShopAndDialogue` |
| `Content/X` | `Content/AnimLibrary` (팩 원본이면 그대로 두고 이름만) |

---

## 3. 이름 변경 목록

### Animation/Player/1HD → Sword

| 지금 | 바꿀 이름 | 위치 |
|---|---|---|
| `Anim_Sword_*` (Attack01~14, Draw, Sheathe, Hit, Idle, Run_*, Walk_*, Skill_ShieldDash) | `A_Sword_*` (뒤는 그대로) | 각 하위 폴더 그대로 |
| `Anim_Sword_Idle1` | `A_Sword_Idle02` (안 쓰면 삭제) | Sword/Move |
| `atk3`, `atk4` | 용도 확인 후 `A_Sword_Attack15`, `A_Sword_Attack16` 또는 삭제 | Sword/Attack |
| `SK_Mannequin_Sequence` | 애니메이션 시퀀스인데 `SK_` 접두어. 용도 확인 후 `A_Sword_…` 또는 삭제 | |
| `A_Guard_Hit01`, `A_Guard_Hit02` | `A_Sword_Guard_Hit01`, `A_Sword_Guard_Hit02` | Sword/Guard |
| `A_Guard_Break`, `A_Guard_Break_Stun` | `A_Sword_Guard_Break`, `A_Sword_Guard_Break_Stun` | Sword/Guard |
| `A_Guard_End` | `A_Sword_Guard_End`과 겹치면 하나 삭제, 아니면 `A_Sword_Guard_End02` | Sword/Guard |
| `anim_roll_back` / `front` / `left` / `right` | `A_Common_Roll_B` / `F` / `L` / `R` | Common/Dodge |
| `Anim_Dodge_BL` / `BR` / `F` / `L` / `R` | `A_Sword_Dodge_BL` … | Sword/Dodge |
| `Anim_Hit_F01`, `Anim_Hit_B01`, `Anim_Hit_L01` … | `A_Sword_Hit_F01`, `A_Sword_Hit_B01` … | Sword/Hit |
| `Anim_Hit_KnockBack_F/B/L/R`, `Anim_Hit_Heavy_KnockBack_F` | `A_Sword_Hit_KnockBack_F` …, `A_Sword_Hit_Heavy_KnockBack_F` | Sword/Hit |
| `Anim_Death01`, `Anim_Death02` | `A_Common_Death01`, `A_Common_Death02` | Common/Death |
| `Anim_GetUP` | `A_Common_GetUp` | Common |
| `Anim_Jump_Start` / `Loop` / `Landing` | `A_Common_Jump_Start` / `Loop` / `Land` | Common/Jump |
| `Anim_Stun_Loop` | `A_Common_Stun_Loop` | Common |
| `BS_Sword_Move_8Way` (1HD 바로 아래) | 이름 그대로 | Sword/Move |

> 피격·회피를 대검도 같이 쓰고 있다면 `Sword` 대신 `Common`으로.

### Animation/Player/2HD → GreatSword

| 지금 | 바꿀 이름 |
|---|---|
| `AM_GS_*` (전부) | `AM_GreatSword_*` |
| `A_GS_*` (전부) | `A_GreatSword_*` |
| `A_GS_Atk1` | `A_GreatSword_Attack01`과 같은 동작이면 삭제, 아니면 `A_GreatSword_Attack00` |
| `AM_GS_Death`, `A_GS_Death`, `AM_GS_Rebirth`, `A_GS_Rebirth` (Move 폴더) | 이름은 위 규칙대로, 위치는 `GreatSword/Death` |
| `AM_GS_Buff_Berserk`, `AM_GS_Buff_Tranquility` | `AM_GreatSword_Buff_Berserk`, `AM_GreatSword_Buff_Tranquility` |
| `Retarget/GreatSword_Turn_L90_Montage` 등 턴 몽타주 4개 | `AM_GreatSword_Turn_L90`, `_R90`, `_L180`, `_R180` (`GreatSword/Move`로) |
| `Retarget/GreatSword_Hit_*_Montage`, `*_Attack09/11/12_Montage` (쓰는 경우) | `AM_GreatSword_…` 로 바꿔서 해당 폴더로 |
| `Retarget` 폴더 나머지 (원본 시퀀스) | 이름 그대로, 폴더만 `GreatSword/_Source` |
| `A_hm_2hd_MF_KB1` ~ `KB3` | 쓰면 `A_GreatSword_KnockBack01`~`03`, 안 쓰면 `_Source`로 |
| `A_hm_2hd_MF_KD1` ~ `KD3` / `TU1` ~ `TU2` / `DT` | `A_GreatSword_KnockDown01`~ / `A_GreatSword_GetUp01`~ / `A_GreatSword_Death02` |
| `A_hm_2hd_MF_KB1_Montage`, `A_hm_2hd_MF_KD1_Montage` | `AM_GreatSword_KnockBack01`, `AM_GreatSword_KnockDown01` |
| `A_hm_2hd_atk2_r`, `A_hm_2hd_atk3_r` | 쓰면 `A_GreatSword_Attack…`, 아니면 `_Source` |
| `A_hm_2hd_start_run`, `A_hm_2hd_end_run`, `A_hm_2hd_rebirth` | `A_GreatSword_Run_Start`, `A_GreatSword_Run_End`, `_Source`(Rebirth 중복) |

### Animation/Player/Staff

| 지금 | 바꿀 이름 |
|---|---|
| `Am_Staff_Draw`, `Am_Staff_Sheathe` | `AM_Staff_Draw`, `AM_Staff_Sheathe` |
| `Anim_Staff_*` | `A_Staff_*` |
| `A_staff_cast_Idle`, `A_staff_cast_start` | `A_Staff_Cast_Idle`, `A_Staff_Cast_Start` |
| `Anim_Staff_Idle` (Staff 바로 아래 + Move 안에 같은 이름 2개) | 하나는 삭제, 남는 것 `A_Staff_Idle` (Move) |
| `AM_Staff_Casting` | `AM_Staff_Cast_Loop` (또는 그대로) |
| 캐스팅 관련 (`Cast_*`) | `Staff/Cast` 폴더로 |

### Animation/Player/Common

| 지금 | 바꿀 이름 |
|---|---|
| `A_None_Idle` | `A_Unarmed_Idle` (스탠스 enum이 `Unarmed`) |
| `MM_*`, `BS_MM_WalkRun` | Epic 원본. 그대로 |

### Animation/Enemy

| 지금 | 바꿀 이름 |
|---|---|
| `Dummy/ABP_EnemyKwang` | `ABP_Dummy` |
| `Dummy/Hitreact_Fwd` / `Bwd` / `Left` / `Right` | `A_Dummy_Hit_F` / `B` / `L` / `R` |
| `Dummy/Hitreact_Fwd_Montage1`, `Hitreact_Bwd_Montage` … | `AM_Dummy_Hit_F`, `AM_Dummy_Hit_B` … |
| `Dummy/PrimaryAttack_C_Slow`, `…_Montage` | `A_Dummy_Attack01`, `AM_Dummy_Attack01` |
| `Dummy/Anim_Enemy_Falldown_*` | `A_Dummy_Falldown_*` |
| `Dummy/AM_Enemy_Falldown_F`, `AM_Knockdown_B`, `AM_Death` | `AM_Dummy_Falldown_F`, `AM_Dummy_KnockDown_B`, `AM_Dummy_Death` |
| `Dummy/Anim_Stun_Loop`, `Anim_Stun_Loop_Montage` | `A_Dummy_Stun_Loop`, `AM_Dummy_Stun_Loop` |
| `Golem/A_Golem_idle`, `A_Golem_walk` | `A_Golem_Idle`, `A_Golem_Walk` |
| `Golem/BS1D_Golem_IdleToWalk` | `BS_Golem_IdleToWalk` (1D 표시는 선택) |

### Characters / Equipment

| 지금 | 바꿀 이름 |
|---|---|
| `Characters/Enemy/Boss/Golem/SKM_Stone_Golem` | `SKM_Golem` (애니메이션·DA·BP가 모두 `Golem`) |
| `SK_Stone_Golem_Skeleton` | `SK_Golem` |
| `PA_Stone_Golem_PhysicsAsset` | `PA_Golem` |
| `CR_Stone_Golem_ModularRig` | `CR_Golem` |
| `M_Stone_Golem`, `MI_Stone_Golem_Inst`, `…Inst1`, `…Inst2` | `M_Golem`, `MI_Golem`, `MI_Golem_02`, `MI_Golem_03` |
| `T_golem_BaseColor` 등 소문자 | `T_Golem_BC`, `T_Golem_N`, `T_Golem_ORM` (무기 텍스처와 같은 방식) |
| `T_cav_dirt_curva`, `T_eye_pos` | `T_Golem_Dirt`, `T_Golem_EyePos` |
| `Characters/Enemy/SKM_Manny_Enemy` | `SKM_Dummy` (`Characters/Enemy/Dummy/`) |

### Blueprints / Abilities

| 지금 | 바꿀 이름 | 이유 |
|---|---|---|
| `GA/Player/Skill/GA_Clecric_Skill` | `GA_Cleric_Skill` | 오타 |
| `GA/Enemy/GA_EnemyAttack_Base` | `GA_Enemy_Attack_Base` | `GA_대상_동작` 순서 |
| `GA/Enemy/GA_EnemyTurn_Base` | `GA_Enemy_Turn_Base` | |
| `GA/Enemy/GA_HitReaction_Enemy` | `GA_Enemy_HitReaction` | |
| `GA/Player/GA_HitReaction_Player` | `GA_Player_HitReaction` | `GA_Player_Death`와 같은 순서 |
| `GE/GE_Levelup` | `GE_LevelUp` | `GC_LevelUp`과 맞춤 |
| `GE/GE_EnemyStatInit` | `GE_Enemy_StatInit` | |
| `GC_LevelUp` (Abilities 바로 아래) | 그대로, `GameplayCues/`로 이동 | |

### Maps / Sound / VFX / 기타

| 지금 | 바꿀 이름 |
|---|---|
| `Maps/AnimTestMap`, `Maps/TestEnvironment` | `LV_AnimTest`, `LV_TestEnvironment` |
| `Maps/NewMaterial`, `NewMaterial_Inst` | 쓰는 곳 확인 후 삭제, 쓰면 `M_…`로 이름 짓고 `Environment`로 |
| `Sound/Combat/Sword_*` | `SW_Sword_Attack01` 식으로 (`_1` → `01`) |
| `Sound/FootStep/Dirt_Walk_1` … | `SW_Footstep_Dirt_Walk01` … |
| `Sound/332661__reitanna__big-thud` | `SW_BigThud` |
| `Sound/Single_footstep_of_g__…`, `footstep-01`, `Step1`~`Step5`, `Sand`, `Snow` | `SW_Footstep_…` (안 쓰는 것은 삭제) |
| `Sound/NewMetaSoundSource` | `MS_…` (용도에 맞게) |
| `VFX/Mateirals/M_` | 이름 없음. 용도에 맞게 (`M_VFX_…`) |
| `VFX/Mateirals/M_VFX_Decall` | `M_VFX_Decal` |
| `UI/Panels/Inventory/EInvFilter` | `E_InvFilter` |

---

## 4. 작업 순서와 주의

1. **반드시 에디터 콘텐츠 브라우저에서** 이름 변경·이동한다 (탐색기에서 바꾸면 참조가 깨짐).
2. 폴더 하나 끝날 때마다 해당 폴더 우클릭 → **Fix Up Redirectors**.
3. 다 끝나면 전체 저장 → 에디터 재시작 → PIE로 무기별 공격·가드·회피·피격 한 번씩 확인.
4. **아이템 ID(데이터 테이블 행 이름)는 바꾸지 않는다.** 예: `Greatsword_Bronze`. 세이브 파일이 ID로 아이템을 저장하므로 바꾸면 기존 세이브의 아이템이 사라진다.
5. C++에 박혀 있던 에셋 경로는 모두 없앴다 (2026-11-24). 값은 BP Class Defaults에서 지정하므로 에셋을 옮기거나 이름을 바꿔도 참조가 따라간다.
   남은 문자열은 맵 이동 주소(`/Game/Non/Maps/LV_StartMap`, `/Game/Non/Maps/<맵이름>`)뿐이다. 맵 이름을 바꾸면 알려줘.
