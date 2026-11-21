# Non 스킬 명세서 (Skill_Spec v0.4 템플릿)
* 상위 문서: Non_Game_Architecture.txt (v3.6) — 태그/공통 규칙은 상위 문서를 따른다.
* 이 문서의 표는 그대로 CSV로 옮겨 `DT_SkillData`(DataTable)로 임포트하는 것을 전제로 한다.
* 표기: [확정] / [제안] / [TBD]. 빈 칸 = 아직 미정.

## 0. 공통 작성 규칙
* 클래스당 스킬 구성 기준 [제안]: 액티브 8개(궁극기 1 포함) + 패시브 6개 + 긴급 회피기 1개.
  * 액티브 구성 권장: 주력 딜 2~3 / 기동·회피 1 / 유틸·CC 1~2 / 자가 회복·생존 1 / 궁극기 1
* 모든 클래스는 전투 중 자가 회복/피해 흡수 수단 1개 이상 필수 (Architecture 3장).
* 연계 스킬 유예시간: 2~3초 (Combat.ChainReady).
* 무기 조건: 모든 클래스 스킬은 RequiredWeaponTags를 가짐. 조건 무기를 들지 않으면 아이콘 회색 + 발동 불가. 각 클래스 표는 섹션 제목의 "기본 무기"를 적용하고, 다른 경우만 Notes에 기재.
  * 긴급 회피기(ComboBreaker)도 무기 조건 적용.
  * 예외(무기 조건 없음): 순수 자기 버프로 지정한 스킬만 (Notes에 "무기 무관" 표기).
* 발도 조건: 모든 클래스 스킬은 State.Weapon.Drawn 필요 (Architecture 3.7).
* 소모 자원: 스킬마다 CostType(Stamina / Mana / Health) 지정. Health 소모는 HP 1 미만으로 내려가지 않음.
* 피해 타입: Physical → 물리 공격력, Magical → 마법 공격력 사용. 힐은 마법 공격력 비례.
* 패시브 목록에 클래스별 [재질 마스터리] 1개 이상 포함.
* 발사 방식은 Architecture 5장의 유형 A(직선) / B(베지어 유도) / C(지면 캐스팅) 중 하나로 표기. 근접은 `Melee`, 자신 중심은 `Self`.

## 1. 컬럼 정의

| 컬럼 | 설명 | 예시 |
|---|---|---|
| SkillId | 고유 ID (RowName) | Def_ShieldBash |
| AbilityTag | Ability.Class.<Class>.<Skill> | Ability.Class.Defender.ShieldBash |
| Name | 표시 이름 | 방패 밀치기 |
| Type | Active / Passive / Ultimate / ComboBreaker | Active |
| Role | Damage / Mobility / CC / Sustain / Aggro / Buff / Heal | Aggro |
| Delivery | Melee / Self / A / B / C | Melee |
| DamageType | Damage.Type.Physical / Magical / None | Physical |
| Element | Damage.Element.* / None | None |
| Coefficient | 해당 공격력(물리/마법) 대비 계수 (Lv1) | 1.20 |
| CoeffPerLevel | 레벨당 계수 증가 | 0.10 |
| MaxLevel | 최대 스킬 레벨 | 5 |
| Cost | 소모량 | 15 |
| CostType | Stamina / Mana / Health | Stamina |
| Cooldown | 초 | 8 |
| CastTime | 초 (0 = 즉발) | 0 |
| bCanMoveWhileCasting | Movement.CastWalk 여부 | false |
| Range | cm | 250 |
| Radius | cm (범위기) | 0 |
| RequiredWeaponTags | 발동 필요 무기 (비우면 섹션 기본값) | Weapon.Type.Shield |
| HitReaction | 히트 리액션 등급 None / Light / Heavy. 넘어짐·띄우기 등은 CCTag로 지정 (CC가 있으면 HitReaction 무시) | Heavy |
| CCTag | 부여 CC 태그 | State.CrowdControl.Stun |
| CCDuration | 초 | 1.5 |
| GroggyBuildup | 보스 그로기 누적량 | 15 |
| ThreatMultiplier | 어그로 배율 (기본 1.0) | 2.0 |
| ChainNext | 연계 2단계 SkillId | |
| Prerequisite | 선행 스킬 (SkillId:Lv) | |
| bSuperArmor | 시전 중 슈퍼아머 | false |
| Montage | 몽타주 경로 | |
| Notes | 비고 | |

## 2. 클래스별 스킬 표

### 2.1 디펜더 (Defender) — 탱커 / 주 자원 스태미나 / 기본 무기 Weapon.Type.Sword1H + Weapon.Type.Shield
| SkillId | Name | Type | Role | Delivery | DamageType | Coefficient | Cost | Cooldown | CCTag / Duration | GroggyBuildup | ThreatMultiplier | ChainNext | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Def_ShieldBash | 방패 밀치기 | Active | Aggro/CC | Melee | Physical | | | | | | | | 적중 시 보너스 어그로 |
| Def_ShieldCharge | 방패 돌격 | Active | Mobility/CC | Melee | Physical | | | | Stun / | | | | |
| Def_GiantShield | 거대 수호의 방패 | Active | Buff | Self | None | | | | | | | | 등 뒤 State.SafeZone, 이동 차단 |
| Def_EarthJudgment | 대지의 심판 | Active | Aggro | Self | Physical | | | 12~15 [제안] | | | | | 최상위 어그로 + 4~5초 고정 |
| Def_TauntingShout | 도발의 외침 | Active | Aggro | Self | None | | | 12~15 [제안] | | | | | 최상위 어그로 + 4~5초 고정 |
| Def_IronStance | 철벽 자세 | Active | Sustain | Self | None | | | | | | | | 막아낸 피해 일부 HP 회복 [제안] |
| Def_Neutralize | 무력화 | Active | CC | Melee | Physical | | | | | | | | 적중 시 보너스 어그로 + 그로기 |
| Def_Intercept | 가로채기 | Active | Aggro | Melee | None | | | | | | | | 신규: 아군 앞으로 돌진해 대신 막음 |
| Def_CB_ShieldBastion | 방패 장벽 전개 | ComboBreaker | Sustain | Self | None | | | | Knockback / | | | | 3초 가드 100%. 모션: Architecture 3.6 |
| Def_P_PlateMastery | 판금 마스터리 | Passive | Buff | | | | | | | | | | 판금 3부위 이상 시 장점 +10%, 단점 50% 완화 [제안] |
| Def_P_GuardSuccess | (패시브) | Passive | | | | | | | | | | | Buff.Passive.Defender.GuardSuccess |
| Def_Ult_ | [TBD] | Ultimate | | | | | | | | | | | |

### 2.2 버서커 (Berserker) — 근접 딜러 / 주 자원 스태미나(일부 체력) / 기본 무기 Weapon.Type.Greatsword
| SkillId | Name | Type | Role | Delivery | DamageType | Coefficient | Cost | Cooldown | CCTag / Duration | GroggyBuildup | ThreatMultiplier | ChainNext | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Ber_DownwardSlash | 내려치기 | Active | Damage/CC | Melee | Physical | | | | Knockback / | | | Ber_ChainStrike | |
| Ber_ChainStrike | 연쇄 타격 | Active | Damage | Melee | Physical | | | | | | | | 연계 2단계 |
| Ber_OverlordCharge | 패왕 돌진 | Active | Mobility | Melee | Physical | | | | | | | | |
| Ber_LeapDodgeSlash | 도약 회피 베기 | Active | Mobility | Melee | Physical | | | | | | | | 회피공격기 (초반 무적) |
| Ber_SpinningAnnihilation | 참마대회전 | Active | Damage | Melee | Physical | | | | | | | | Movement.CastWalk |
| Ber_Smash | 강타 | Active | Damage | Melee | Physical | | | | | | | | |
| Ber_Rage | 광포화(가칭) | Active | Buff | Self | None | | | | | | | | Buff.Berserker.Rage 부여. 공격력↑ / 받는 피해↑. 무기 무관 |
| Ber_Calm | 평정(가칭) | Active | Sustain | Self | None | | | ≥ 광포화 지속시간 [제안] | | | | | 광포화 중: 광포화 해제 + 최대 HP 10/12/15% + 스태미나 회복 / 광포화 없음: 스태미나만 회복. 무기 무관 |
| Ber_Bloodthirst | 피의 갈망 | Active | Damage | Melee | Physical | | | | | | | | 역할 재검토 (자가 회복은 평정이 담당) |
| Ber_P_BloodJoy | 피의 환희(가칭) | Passive | Sustain | | | | | | | | | | 치명타 시 최대 HP 0.5~1% 확정 회복, 내부 쿨 1초 [제안] |
| Ber_Mad | 광란 | Ultimate | Damage | Melee | Physical | | | | | | | | |
| Ber_CB_GiantsLeap | 야수의 도약 | ComboBreaker | Sustain | Self | None | | | | | | | | |
| Ber_P_ExtremeFervor | (패시브) | Passive | | | | | | | | | | | Buff.Passive.Berserker.ExtremeFervor |

### 2.3 어쌔신 (Assassin) — 근접 딜러 / 주 자원 스태미나 / 기본 무기 Weapon.Type.Dagger
| SkillId | Name | Type | Role | Delivery | DamageType | Coefficient | Cost | Cooldown | CCTag / Duration | GroggyBuildup | ThreatMultiplier | ChainNext | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Asn_ShadowSlash | 그림자 베기 | Active | Damage | Melee | Physical | | | | | | | | |
| Asn_StealthAmbush | 은신 기습 | Active | Damage | Melee | Physical | | | | | | | | State.Stealth |
| Asn_P_Backstab | 배후 급습(가칭) | Passive | Sustain | | | | | | | | | | 백어택(후방 120°) 적중 시 확정 치명타 + 피해 일부 흡혈, 내부 쿨 [제안]. Event.BackAttack |
| Asn_PhantomStep | 환영 스텝 | Active | Mobility | Self | None | | | | | | | | I-Frame |
| Asn_ThousandBlades | 천 개의 칼날 | Ultimate | Damage | Melee | Physical | | | | | | | | |
| Asn_ [TBD] | | Active | | | | | | | | | | | 3~4개 추가 필요 |
| Asn_CB_ShadowSlip | 그림자 소멸 | ComboBreaker | Mobility | Self | None | | | | | | | | 공격자 배후 순간이동 |
| Asn_P_ShadowClone | (패시브) | Passive | | | | | | | | | | | Buff.Passive.Assassin.ShadowClone |

### 2.4 소서러 (Sorcerer) — 원거리 딜러 / 주 자원 마나 / 기본 무기 Weapon.Type.Staff
| SkillId | Name | Type | Role | Delivery | DamageType | Coefficient | Cost | Cooldown | CCTag / Duration | GroggyBuildup | ThreatMultiplier | ChainNext | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Sor_FireArrow | 화염 화살 | Active | Damage | A | Magical | | | | | | | | Element.Fire |
| Sor_ArcaneOrb | 비전 구체 | Active | Damage | B | Magical | | | | | | | | 확정 유도 |
| Sor_MeteorCasting | 메테오 | Active | Damage | C | Magical | | | | | | | | Element.Fire |
| Sor_Blink | 점멸 | Active | Mobility | Self | None | | | | | | | | |
| Sor_Apocalypse | 종말 | Ultimate | Damage | C | Magical | | | | | | | | |
| Sor_ [TBD] | | Active | | | | | | | | | | | 빙결 계열 등 3개 추가 필요 |
| Sor_CB_IceTomb | 얼음 감옥 | ComboBreaker | Sustain | Self | Magical | | | | Knockback / | | | | 무적 중 HP/MP 회복안은 보류 |
| Sor_P_ManaResonance | (패시브) | Passive | | | | | | | | | | | Buff.Passive.Sorcerer.ManaResonance |

### 2.5 아처 (Archer) — [클래스 설계 보류]
* 아래 표는 기존 태그 보존용이며 수치 작성 대상에서 제외.
| SkillId | Name | Type | Role | Delivery | DamageType | Coefficient | Cost | Cooldown | CCTag / Duration | GroggyBuildup | ThreatMultiplier | ChainNext | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Arc_PiercingArrow | 관통 화살 | Active | Damage | A | Physical | | | | | | | | |
| Arc_PrecisionExplosive | 정밀 폭발 화살 | Active | Damage | A | Physical | | | | | | | | Element.Fire |
| Arc_BackflipShot | 백플립 사격 | Active | Mobility | A | Physical | | | | | | | | |
| Arc_ArrowRain | 화우의 세례 | Active | Damage | C | Physical | | | | | | | | |
| Arc_JustRelease | 저스트 릴리즈 | Active | Damage | A | Physical | | | | | | | | 타이밍 보너스 |
| Arc_ [TBD] | | Active | | | | | | | | | | | 덫/궁극기 등 추가 필요 |
| Arc_CB_VaultShot | 후방 폭발 사격 | ComboBreaker | Mobility | Self | Physical | | | | | | | | Debuff.Slow |
| Arc_P_WeakPointLock | (패시브) | Passive | | | | | | | | | | | Buff.Passive.Archer.WeakPointLock |
| Arc_P_NaturesBreath | 자연의 숨결 | Passive | Sustain | | | | | | | | | | 5초 지속 재생 [제안] |

### 2.6 클레릭 (Cleric) — 힐러/지원 / 주 자원 마나 / 기본 무기 Weapon.Type.Mace 또는 Weapon.Type.Staff [TBD]
| SkillId | Name | Type | Role | Delivery | DamageType | Coefficient | Cost | Cooldown | CCTag / Duration | GroggyBuildup | ThreatMultiplier | ChainNext | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Clr_Heal | 치유 | Active | Heal | Self | None | | | | | | | | 자신 중심 범위힐 |
| Clr_Tranquility | 평온 | Active | Heal | Self | None | | | | | | | | 반경 지속힐 + 정화 |
| Clr_SanctuaryOfLife | 생명의 성역 | Active | Heal | C | None | | | | | | | | 지면 장판힐 |
| Clr_PurifyingLight | 정화의 빛 | Active | Damage | B | Magical | | | | | | | | Element.Holy |
| Clr_AngelWings | 천사의 날개 | Active | Mobility | Self | None | | | | | | | | |
| Clr_Aura | 오라 | Active | Buff | Self | None | | | | | | | | 반경 8~12m, 방어/스태미나/공격 토글 [제안] |
| Clr_GoddessBlessing | 여신의 축복 | Ultimate | Heal | Self | None | | | | | | | | 범위 부활 + 회복 |
| Clr_ [TBD] | | Active | | | | | | | | | | | 신성 딜 장판 등 |
| Clr_CB_HolyBarrier | 신의 가호 | ComboBreaker | Sustain | Self | None | | | | | | | | [보류] |
| Clr_P_ [TBD] | | Passive | | | | | | | | | | | 고유 패시브 확정 대기 |

## 3. 작성 체크리스트
* [ ] 클래스별 액티브/패시브 개수 기준 확정
* [ ] 클래스별 재질 마스터리 패시브 지정
* [ ] 모든 스킬 RequiredWeaponTags / HitReaction 입력
* [ ] 모든 스킬 Coefficient / Cost / Cooldown 1차 수치 입력
* [ ] 연계(ChainNext) 및 선행(Prerequisite) 관계 확정
* [ ] 클래스별 DPS 기대값 비교 (딜러 간 ±10% 이내 [제안])
* [ ] 스킬 이름 한글/영문 표기 통일
