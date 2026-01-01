#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Character/NonCharacterBase.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"



static FName GetDefaultSocketForSlot(EEquipmentSlot Slot)
{
    switch (Slot)
    {
    case EEquipmentSlot::WeaponMain: return FName(TEXT("hand_r_socket"));
    case EEquipmentSlot::WeaponSub:  return FName(TEXT("hand_l_socket"));
    case EEquipmentSlot::Head:     return FName(TEXT("head_socket"));
    case EEquipmentSlot::Chest:      return FName(TEXT("chest_socket"));
    case EEquipmentSlot::Hands:     return FName(TEXT("hand_r_socket")); // 필요 시 변경
    case EEquipmentSlot::Feet:      return FName(TEXT("foot_r_socket")); // 필요 시 변경
    default:                         return NAME_None;
    }
}

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

USkeletalMeshComponent* UEquipmentComponent::GetOwnerMesh() const
{
    if (const ACharacter* C = Cast<ACharacter>(GetOwner()))
    {
        return C->GetMesh();
    }
    // 캐릭터가 아니면 첫 번째 스켈레탈 메쉬 컴포넌트를 찾아 사용
    return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

bool UEquipmentComponent::EquipFromInventory(UInventoryComponent* Inv, int32 FromIndex, EEquipmentSlot OptionalTarget)
{
    if (!Inv) return false;

    UInventoryItem* Item = Inv->GetAt(FromIndex);
    if (!Item) return false;

    const FItemRow& Row = Item->CachedRow;
    const EEquipmentSlot Target = (OptionalTarget != EEquipmentSlot::None) ? OptionalTarget : Row.EquipSlot;

    FText Fail;
    if (!CanEquip(Item, Target, Fail)) return false;

    // (A) 먼저 한 칸 확보: FromIndex를 비워둔다.
    Inv->RemoveAt(FromIndex, 1);

    // (B) 장착 시도 (교체품은 EquipInternal 내부에서 OwnerInventory->AddItem으로 되돌아감)
    int32 ReturnedIdx = INDEX_NONE;
    const bool bEquipped = EquipInternal(Item, Target, &ReturnedIdx);
    if (!bEquipped)
    {
        // (C) 장착 실패 → 롤백: 원래 아이템을 FromIndex에 되돌린다.
        int32 PutIdx = INDEX_NONE;
        if (Inv->AddItem(Item->ItemId, Item->Quantity, PutIdx))
        {
            if (PutIdx != FromIndex)
            {
                Inv->Move(PutIdx, FromIndex);
            }
        }
        else
        {
        }
        return false;
    }

    // (D) 교체품이 인벤토리 어딘가(PutIdx)에 들어갔다면 → 사용자가 뺀 자리(FromIndex)로 정렬
    if (ReturnedIdx != INDEX_NONE && ReturnedIdx != FromIndex)
    {
        if (!Inv->Move(ReturnedIdx, FromIndex))
        {
            Inv->Swap(ReturnedIdx, FromIndex);
        }
    }

    return true;
}

bool UEquipmentComponent::EquipItem(UInventoryItem* Item, EEquipmentSlot OptionalTarget)
{
    if (!Item) return false;
    const FItemRow& Row = Item->CachedRow;
    const EEquipmentSlot Target = (OptionalTarget != EEquipmentSlot::None) ? OptionalTarget : Row.EquipSlot;

    FText Fail;
    if (!CanEquip(Item, Target, Fail)) return false;

    return EquipInternal(Item, Target);
}

bool UEquipmentComponent::Unequip(EEquipmentSlot Slot)
{
    if (!Equipped.Contains(Slot)) return false;

    UnequipInternal(Slot);
    return true;
}

bool UEquipmentComponent::UnequipToInventory(EEquipmentSlot Slot, int32& OutInventoryIndex)
{
    OutInventoryIndex = INDEX_NONE;

    UInventoryItem* Cur = GetEquippedItemBySlot(Slot);
    if (!Cur) return false;

    // 인벤이 없으면 일단 그냥 해제(아이템 유실 방지 목적이면 false를 리턴하는 선택지도 가능)
    if (!OwnerInventory)
    {
        UnequipInternal(Slot); // OnUnequipped 브로드캐스트 됨
        return true;
    }

    // 먼저 인벤에 자리 확보 시도 (실패하면 해제하지 않음)
    int32 PutIdx = INDEX_NONE;
    const bool bAdded = OwnerInventory->AddItem(Cur->ItemId, FMath::Max(1, Cur->Quantity), PutIdx);
    if (!bAdded)
    {
        // OnAddFailed 은 InventoryComponent 쪽에서 브로드캐스트 되어 토스트가 뜸
        return false;
    }

    // 인벤에 정상 추가되었으니 실제 해제
    UnequipInternal(Slot); // 비주얼 제거 + OnUnequipped 브로드캐스트

    OutInventoryIndex = PutIdx;
    return true;
}

UInventoryItem* UEquipmentComponent::GetEquippedItemBySlot(EEquipmentSlot Slot) const
{
    if (const TObjectPtr<UInventoryItem>* Found = Equipped.Find(Slot))
    {
        return Found->Get();
    }
    return nullptr;
}

bool UEquipmentComponent::ReattachSlotToSocket(EEquipmentSlot Slot, FName NewSocket, const FTransform& Relative)
{
    USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
    if (!OwnerMesh) return false;

    TObjectPtr<UMeshComponent>* Found = VisualComponents.Find(Slot);
    if (!Found) return false;

    if (UMeshComponent* MC = Found->Get())
    {
        MC->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewSocket);
        MC->SetRelativeTransform(Relative);

        if (FEquipmentVisual* VS = VisualSlots.Find(Slot))
        {
            VS->Socket = NewSocket;
            VS->Relative = Relative;
        }
        else
        {
            FEquipmentVisual NewVS;
            NewVS.Socket = NewSocket;
            NewVS.Relative = Relative;
            VisualSlots.Add(Slot, NewVS);
        }
        return true;
    }
    return false;
}

UMeshComponent* UEquipmentComponent::GetVisualComponent(EEquipmentSlot Slot) const
{
    if (const TObjectPtr<UMeshComponent>* Found = VisualComponents.Find(Slot))
    {
        return Found->Get();
    }
    return nullptr;
}

// ==================== 규칙 검사 ====================

bool UEquipmentComponent::CanEquip(const UInventoryItem* Item, EEquipmentSlot TargetSlot, FText& OutFailReason) const
{
    OutFailReason = FText::GetEmpty();
    if (!Item) { OutFailReason = FText::FromString(TEXT("Invalid item")); return false; }

    const FItemRow& Row = Item->CachedRow;

    // 반드시 장비 타입이어야 함
    if (Row.ItemType != EItemType::Equipment)
    {
        OutFailReason = FText::FromString(TEXT("Not equipment"));
        return false;
    }

    // 슬롯 호환
    if (TargetSlot == EEquipmentSlot::None)
    {
        OutFailReason = FText::FromString(TEXT("No target slot"));
        return false;
    }

    // 추가로 직업 제한/레벨 제한 등 규칙이 있다면 여기서 검사
    // (예: if (PlayerLevel < Row.RequiredLevel) return false;)

    return true;
}

// ==================== 내부 장착/해제 ====================

bool UEquipmentComponent::EquipInternal(UInventoryItem* Item, EEquipmentSlot TargetSlot, int32* OutReturnedIndex /*=nullptr*/)
{
    UInventoryItem* Replaced = nullptr;

    // '장착 이전에 메인무기가 있었나?' 스냅샷
    const bool bHadMainBefore =
        (TargetSlot == EEquipmentSlot::WeaponMain) &&
        (GetEquippedItemBySlot(EEquipmentSlot::WeaponMain) != nullptr);

    // 기존 동일 슬롯에 뭔가 있으면 효과/비주얼 제거
    if (TObjectPtr<UInventoryItem>* FoundPtr = Equipped.Find(TargetSlot))
    {
        Replaced = FoundPtr->Get();
        if (Replaced)
        {
            RemoveEquipmentEffects(Replaced->CachedRow);
        }
        RemoveVisual(TargetSlot);

        // [New] 어빌리티 회수 (교체 시에도 수행해야 함)
        if (FGrantedAbilityHandles* HandleEntry = GrantedAbilityHandles.Find(TargetSlot))
        {
            if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
            {
                if (UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EquipInternal-Swap] Clearing %d Abilities from Slot: %d"), HandleEntry->Handles.Num(), (int32)TargetSlot);
                    for (const FGameplayAbilitySpecHandle& Handle : HandleEntry->Handles)
                    {
                        ASC->ClearAbility(Handle);
                    }
                }
            }
            GrantedAbilityHandles.Remove(TargetSlot);
        }
    }

    // 새 아이템 등록
    Equipped.Add(TargetSlot, Item);

    // === "지금 장착한 쪽"을 우선으로 상호배타 정리 (2H/Staff ↔ WeaponSub) ===================
    auto IsTwoHandOrStaff = [](const UInventoryItem* It) -> bool
        {
            return It && (It->IsTwoHandedWeapon()
                || It->CachedRow.WeaponType == EWeaponType::Staff); // 프로젝트에 맞게 Staff 판정 보강
        };

    if (TargetSlot == EEquipmentSlot::WeaponSub)
    {
        // 서브(방패) 장착 시도 → 메인에 2H/Staff 있으면 메인을 해제하여 "서브 장착을 우선"
        if (UInventoryItem* Main = GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
        {
            if (IsTwoHandOrStaff(Main))
            {
                int32 DummyIdx = INDEX_NONE;
                if (!UnequipToInventory(EEquipmentSlot::WeaponMain, DummyIdx))
                {
                    UnequipInternal(EEquipmentSlot::WeaponMain); // 규칙 강제
                }
            }
        }
    }
    else if (TargetSlot == EEquipmentSlot::WeaponMain)
    {
        // 메인 장착 시도 → 새 메인이 2H/Staff라면 서브(방패)를 해제하여 "메인 장착을 우선"
        if (IsTwoHandOrStaff(Item))
        {
            if (GetEquippedItemBySlot(EEquipmentSlot::WeaponSub))
            {
                int32 DummyIdx = INDEX_NONE;
                if (!UnequipToInventory(EEquipmentSlot::WeaponSub, DummyIdx))
                {
                    UnequipInternal(EEquipmentSlot::WeaponSub);
                }
            }
        }
    }
    // =====================================================================================================

    // Row.AttachSocket이 1순위가 되도록 비주얼 오버라이드 캐시 삭제
    VisualSlots.Remove(TargetSlot);

    // 충돌 정리로 인해 방금 등록한 아이템이 제거되었는지 안전 체크
    const bool bStillEquippedHere = (GetEquippedItemBySlot(TargetSlot) == Item);
    if (bStillEquippedHere)
    {
        // 비주얼 생성/부착
        ApplyVisual(TargetSlot, Item->CachedRow);

        // 효과 적용/세트 보너스 갱신
        ApplyEquipmentEffects(Item->CachedRow);
        RecomputeSetBonuses();

        // 홈소켓 캐시 재계산
        RecomputeHomeSocketFromEquipped(TargetSlot);

        // [New] 어빌리티 부여 (GrantedAbilities)
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            if (UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent())
            {
                // 1) 개별 아이템 지정 (GrantedAbilities)
                if (Item->CachedRow.GrantedAbilities.Num() > 0)
                {
                    FGrantedAbilityHandles& HandleEntry = GrantedAbilityHandles.FindOrAdd(TargetSlot);
                    
                    for (const TSubclassOf<UGameplayAbility>& AbilityClass : Item->CachedRow.GrantedAbilities)
                    {
                        if (AbilityClass)
                        {
                            FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, Char);
                            FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
                            HandleEntry.Handles.Add(Handle);
                        }
                    }
                }

                // 2) [New] 무기 타입별 자동 지정 (WeaponTypeDefaultAbilities)
                if (const FWeaponDefaultAbilities* FoundSet = WeaponTypeDefaultAbilities.Find(Item->CachedRow.WeaponType))
                {
                    for (const TSubclassOf<UGameplayAbility>& AbilityClass : FoundSet->Abilities)
                    {
                        if (AbilityClass)
                        {
                            FGrantedAbilityHandles& HandleEntry = GrantedAbilityHandles.FindOrAdd(TargetSlot);
                            FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, Char);
                            FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
                            HandleEntry.Handles.Add(Handle);
                        }
                    }
                }
            }
        }

        // 손 재부착/스탠스 갱신은 '메인 슬롯 장착' && '이전에 메인무기 있었음' && '지금도 Armed' 일 때만
        // 손 재부착/스탠스 갱신은 '메인 슬롯 장착' && '이전에 메인무기 있었음' && '지금도 Armed' 일 때만
        // [수정] 방패(Sub) 장착 시에도 즉시 Armed 처리 및 스탠스 갱신 (토글 없음)
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            if (TargetSlot == EEquipmentSlot::WeaponMain)
            {
                const FName HandSocket = Char->GetHandSocketForItem(Item);
                if (bHadMainBefore && Char->IsArmed() && HandSocket != NAME_None)
                {
                    ReattachSlotToSocket(TargetSlot, HandSocket, FTransform::Identity);
                }
                
                // 메인 교체 시: 이미 Armed 상태였다면 스탠스 갱신
                if (bHadMainBefore && Char->IsArmed())
                {
                    Char->RefreshWeaponStance();
                }
            }
            else if (TargetSlot == EEquipmentSlot::WeaponSub)
            {
                // 방패 장착 시: 즉시 Armed로 만들고 스탠스 갱신
                Char->SetArmed(true); 
                // SetArmed 내부에서 RefreshWeaponStance가 호출됨
            }
        }

        // [New] 장비 교체 시 태그 갱신 (메인/서브 상관없이)
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            UpdateWeaponTags(Char->IsArmed());
        }
    }
    else
    {
    }

    // 이벤트 브로드캐스트(최종 슬롯 상태 기준)
    OnEquipped.Broadcast(TargetSlot, GetEquippedItemBySlot(TargetSlot));

    // 교체품이 있었다면 인벤토리로 되돌리기
    if (Replaced)
    {
        if (OwnerInventory)
        {
            int32 ReturnedIdx = INDEX_NONE;
            const FName ReplacedId = Replaced->ItemId;
            const int32 ReplacedQty = FMath::Max(1, Replaced->Quantity);

            const bool bReturned = OwnerInventory->AddItem(ReplacedId, ReplacedQty, ReturnedIdx);
            if (!bReturned)
            {
                // 되돌리기 실패 → 전체 롤백
                if (bStillEquippedHere)
                {
                    RemoveEquipmentEffects(Item->CachedRow);
                    RemoveVisual(TargetSlot);
                }

                // 예전 아이템 복원
                Equipped.Add(TargetSlot, Replaced);
                ApplyVisual(TargetSlot, Replaced->CachedRow);
                ApplyEquipmentEffects(Replaced->CachedRow);
                RecomputeSetBonuses();
                if (OutReturnedIndex) { *OutReturnedIndex = INDEX_NONE; }
                return false;
            }

            if (OutReturnedIndex) { *OutReturnedIndex = ReturnedIdx; }
        }
        else
        {
            if (OutReturnedIndex) { *OutReturnedIndex = INDEX_NONE; }
        }
    }
    else
    {
        if (OutReturnedIndex) { *OutReturnedIndex = INDEX_NONE; }
    }

    return true;
}


void UEquipmentComponent::UnequipInternal(EEquipmentSlot Slot)
{
    if (UInventoryItem* Item = GetEquippedItemBySlot(Slot))
    {
        RemoveEquipmentEffects(Item->CachedRow);
    }

    RemoveVisual(Slot);

    VisualSlots.Remove(Slot);
    SlotHomeSocketMap.Remove(Slot);

    Equipped.Remove(Slot);
    RecomputeSetBonuses();

    // [New] 어빌리티 회수
    if (FGrantedAbilityHandles* HandleEntry = GrantedAbilityHandles.Find(Slot))
    {
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            if (UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent())
            {
                UE_LOG(LogTemp, Warning, TEXT("[UnequipInternal] Clearing %d Abilities from Slot: %d"), HandleEntry->Handles.Num(), (int32)Slot);
                for (const FGameplayAbilitySpecHandle& Handle : HandleEntry->Handles)
                {
                    // 즉시 제거 (혹은 SetRemoveAbilityOnEnd 등 상황에 맞춰 조정)
                    ASC->ClearAbility(Handle);
                }
            }
        }
        GrantedAbilityHandles.Remove(Slot);
    }

    OnUnequipped.Broadcast(Slot);

    if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
    {
        // 메인 무기 해제되면 Armed을 false로
        if (Slot == EEquipmentSlot::WeaponMain && Char->IsArmed())
        {
            Char->SetArmed(false);
        }

        // 최종 스탠스/회전 동기화
        Char->RefreshWeaponStance();
    }
}


void UEquipmentComponent::ResolveTwoHandedConflicts()
{
    // 메인 무기가 2H 또는 Staff면 보조(방패 포함) 사용 불가
    if (UInventoryItem* Main = GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
    {
        const bool bTwoHandOrStaff =
            Main->IsTwoHandedWeapon()
            || (Main->CachedRow.WeaponType == EWeaponType::Staff); // 프로젝트에 맞게 조정

        if (bTwoHandOrStaff)
        {
            if (GetEquippedItemBySlot(EEquipmentSlot::WeaponSub))
            {
                int32 Dummy = INDEX_NONE;
                if (!UnequipToInventory(EEquipmentSlot::WeaponSub, Dummy))
                {
                    // 인벤 꽉 찼으면 규칙 강제 적용(경고 로그만 남기고 해제)
                    UnequipInternal(EEquipmentSlot::WeaponSub);
                }
            }
        }
    }
}

void UEquipmentComponent::ResolveWeaponSubConflicts()
{
    // 보조가 방패면 메인에 2H/Staff 금지
    if (UInventoryItem* Sub = GetEquippedItemBySlot(EEquipmentSlot::WeaponSub))
    {
        const bool bWeaponSub = (Sub->CachedRow.WeaponType == EWeaponType::WeaponSub);
        if (!bWeaponSub) return;

        if (UInventoryItem* Main = GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
        {
            const bool bTwoHandOrStaff =
                Main->IsTwoHandedWeapon()
                || (Main->CachedRow.WeaponType == EWeaponType::Staff); // 프로젝트에 맞게 조정

            if (bTwoHandOrStaff)
            {
                int32 Dummy = INDEX_NONE;
                if (!UnequipToInventory(EEquipmentSlot::WeaponMain, Dummy))
                {
                    UnequipInternal(EEquipmentSlot::WeaponMain);
                }
            }
        }
    }
}


// ==================== 비주얼 ====================

void UEquipmentComponent::ApplyVisual(EEquipmentSlot Slot, const FItemRow& Row) //  FItemRow
{
    RemoveVisual(Slot);

    USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
    if (!OwnerMesh) return;

    // 1) VisualSlots 덮어쓰기 → 2) Row.AttachSocket → 3) 기본 소켓
    FName SocketToUse = NAME_None;
    FTransform Relative = FTransform::Identity;

    // 1) VisualSlots 우선
    if (const FEquipmentVisual* VS = VisualSlots.Find(Slot))
    {
        if (VS->Socket != NAME_None) SocketToUse = VS->Socket;
        Relative = VS->Relative;
    }

    // 2) Row.AttachSocket
    if (SocketToUse == NAME_None && Row.AttachSocket != NAME_None)
    {
        SocketToUse = Row.AttachSocket;
    }

    // 3) 슬롯별 기본
    if (SocketToUse == NAME_None)
    {
        SocketToUse = GetDefaultSocketForSlot(Slot);
    }

    // 메시가 없으면 종료
    if (Row.SkeletalMesh.IsNull() && Row.StaticMesh.IsNull())
    {
        return;
    }

    UMeshComponent* NewVisual = nullptr;

    if (!Row.SkeletalMesh.IsNull())
    {
        if (USkeletalMesh* SK = Row.SkeletalMesh.LoadSynchronous())
        {
            USkeletalMeshComponent* SKC = NewObject<USkeletalMeshComponent>(GetOwner());
            SKC->SetSkeletalMesh(SK);
            SKC->SetupAttachment(OwnerMesh, SocketToUse);
            SKC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SKC->SetGenerateOverlapEvents(false);
            SKC->SetCastShadow(true);
            SKC->RegisterComponent();
            SKC->SetRelativeTransform(Relative);
            NewVisual = SKC;
        }
    }
    else
    {
        if (UStaticMesh* SM = Row.StaticMesh.LoadSynchronous())
        {
            UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(GetOwner());
            SMC->SetStaticMesh(SM);
            SMC->SetupAttachment(OwnerMesh, SocketToUse);
            SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SMC->SetGenerateOverlapEvents(false);
            SMC->SetCastShadow(true);
            SMC->RegisterComponent();
            SMC->SetRelativeTransform(Relative);
            NewVisual = SMC;
        }
    }

    if (NewVisual)
    {
        VisualComponents.Add(Slot, NewVisual);
    }
}
void UEquipmentComponent::RemoveVisual(EEquipmentSlot Slot)
{
    if (TObjectPtr<UMeshComponent>* Found = VisualComponents.Find(Slot))
    {
        if (UMeshComponent* MC = Found->Get())
        {
            MC->DestroyComponent();
        }
        VisualComponents.Remove(Slot);
    }
}



// ==================== 효과(확장용) ====================

void UEquipmentComponent::ApplyEquipmentEffects(const FItemRow& /*Row*/)
{
    // GAS 효과 적용하려면 여기서 처리
}

void UEquipmentComponent::RemoveEquipmentEffects(const FItemRow& /*Row*/)
{
    // GAS 효과 제거
}

void UEquipmentComponent::RecomputeSetBonuses()
{
    // 세트 아이템 개수별 보너스 등
}

// ========= 소켓 관련=======
void UEquipmentComponent::SetHomeSocketForSlot(EEquipmentSlot Slot, FName SocketName)
{
    SlotHomeSocketMap.Add(Slot, SocketName);
}

FName UEquipmentComponent::GetHomeSocketForSlot(EEquipmentSlot Slot) const
{
    if (const FName* Found = SlotHomeSocketMap.Find(Slot))
    {
        return *Found;
    }
    // 폴백: 슬롯이 무기인지에 따라 적당한 기본값
    if (Slot == EEquipmentSlot::WeaponMain || Slot == EEquipmentSlot::WeaponSub)
    {
        return DefaultSheathSocket1H; // 최소 폴백
    }
    return NAME_None;
}

void UEquipmentComponent::RecomputeHomeSocketFromEquipped(EEquipmentSlot Slot)
{
    UInventoryItem* Item = GetEquippedItemBySlot(Slot);
    FName DtSocket = NAME_None;
    bool bTwoHand = false;

    if (Item)
    {
        DtSocket = Item->GetAttachSocket();       // 데이터테이블의 AttachSocket
        bTwoHand = Item->IsTwoHandedWeapon();
    }

    const FName Fallback = bTwoHand ? DefaultSheathSocket2H : DefaultSheathSocket1H;
    const FName Home = (DtSocket != NAME_None) ? DtSocket : Fallback;

    SetHomeSocketForSlot(Slot, Home);
}

void UEquipmentComponent::ReattachSlotToHome(EEquipmentSlot Slot)
{
    const FName Home = GetHomeSocketForSlot(Slot);
    if (Home != NAME_None)
    {
        // Relative 오프셋이 있다면 여기에 적용(현재는 Identity)
        ReattachSlotToSocket(Slot, Home, FTransform::Identity);
    }
}

USceneComponent* UEquipmentComponent::GetVisualForSlot(EEquipmentSlot Slot) const
{
    if (const TObjectPtr<UMeshComponent>* Found = VisualComponents.Find(Slot))
    {
        return Found->Get(); // UMeshComponent 는 USceneComponent 파생
    }
    return nullptr;
}

bool UEquipmentComponent::IsTwoHandedLike(const UInventoryItem* Item) const
{
    if (!Item) return false;

    // 너 프로젝트의 무기 타입 접근으로 교체
    const EWeaponType WT = Item->CachedRow.WeaponType;        // 예: Item->CachedRow.WeaponType;
    switch (WT)
    {
    case EWeaponType::TwoHanded:
    case EWeaponType::Staff:     //  스태프를 양손처럼 취급
        return true;
    default:
        return false;
    }
}


bool UEquipmentComponent::IsMainHandOccupyingBothHands() const
{
    const UInventoryItem* Main = GetEquippedItemBySlot(EEquipmentSlot::WeaponMain);
    return IsTwoHandedLike(Main);
}
#include "System/NonSaveGame.h"

TArray<FEquipmentSaveData> UEquipmentComponent::GetEquippedItemsForSave() const
{
    TArray<FEquipmentSaveData> OutData;
    for (const auto& Pair : Equipped)
    {
        if (const UInventoryItem* Item = Pair.Value)
        {
            FEquipmentSaveData Data;
            Data.Slot = Pair.Key;
            Data.ItemId = Item->ItemId; 
            Data.Quantity = Item->Quantity;
            OutData.Add(Data);
        }
    }
    return OutData;
}

void UEquipmentComponent::RestoreEquippedItemsFromSave(const TArray<FEquipmentSaveData>& InData)
{
    // 기존 장비 해제
    TArray<EEquipmentSlot> SlotsToRemove;
    Equipped.GetKeys(SlotsToRemove);
    for (EEquipmentSlot Slot : SlotsToRemove)
    {
        UnequipInternal(Slot);
    }

    // 복구
    // Note: InventoryComponent가 Owner에 있다면 CreateItem 등을 쓸 수 있겠지만,
    // 여기서는 독립적으로 UObject NewObject로 아이템 생성해야 함.
    // UInventoryItem은 UObject 기반이므로 NewObject<UInventoryItem>(Owner) 가능.
    
    // 데이터 로더 필요 (ItemId -> FItemRow)
    // 보통 InventoryComponent 나 GameInstance/Subsystem을 통해 로드함.
    // 여기서는 Item->ItemId 세팅 후 OnRep/PostLoad 등을 통해 Row 로드...
    // 하지만 UInventoryItem은 보통 Init(ItemId) 함수 등을 가짐.
    
    // UInventoryItem::Init() 같은게 있는지 확인 필요.
    // 확인 결과: UInventoryItem은 보통 Row를 캐싱함. 
    // 여기서는 UInventoryComponent::MakeItem 등 도움을 받거나 직접 생성.

    // 간단히 NewObject 후 ItemId 설정, Row 캐싱은 UInventoryItem 기능 의존.
    // 만약 UInventoryComponent가 없다면 직접 해결해야 함.
    
    // Subsystem 활용? -> GameInstance 접근 가능.
    // 일단 OwnerInventory가 있다면 활용.
    
    UInventoryComponent* InvComp = OwnerInventory; 
    if (!InvComp) InvComp = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;

    for (const FEquipmentSaveData& Data : InData)
    {
        if (Data.ItemId.IsNone()) continue;

        UInventoryItem* NewItem = NewObject<UInventoryItem>(GetOwner());
        NewItem->ItemId = Data.ItemId;
        NewItem->Quantity = Data.Quantity;
        
        // Row 데이터 로드 (필수)
        // 만약 UInventoryComponent에 GetItemRow 같은 헬퍼가 있다면 사용.
        // 없다면 Subsystem 사용.
        // 현재 코드상 UInventoryItem::CachedRow는 public이므로 직접 채워야 할 수도.
        // -> 보통 CreateItem 류 함수가 처리함.
        
        // 일단 GameInstance의 Subsystem(가령 SkillSystemSubsystem 처럼 ItemDataSubsystem이 있는지?)
        // 또는 AssetManager? 
        // NonCharacterBase에서 처리하는게 편할 수도 있지만 여기서는 직접 처리.
        // 임시: InventoryComponent가 있다면 거기서 로딩 위임 (MakeItem 같은게 있다면).
        // 코드를 보니 InventoryComponent -> GetAt -> ... 
        
        // *중요* : Global DataTable 접근이 필요함.
        // NonGameSettings에서 MasterTable을 가져오거나 해야 함.
        // 일단 로드 로직은 생략하고(Row가 없으면 비주얼 안나옴), 
        // *EquipInternal* 호출 시 CanEquip 검사 등에서 Row가 필요함.
        
        // 해결책: SaveGameSubsystem에서 로드할 때 InventoryComponent가 이미 로드되어 있음.
        // InventoryComponent 쪽에 'CreateItem(ItemId)' 헬퍼가 있는게 베스트.
        // 없으면.. 데이터 테이블을 직접 로드해야 함. 
        // 일단 여기서는 "Row 로드" 로직이 필요함을 인지.
        
        // [임시] OwnerInventory가 있으면 거기서 MakeItemLike (가상의) 호출? 
        // (InventoryComponent.cpp에는 AddItem 등이 있음)
        
        // 가장 확실한 방법: GameInstance -> Subsystem -> GetItemData(ItemId)
        // 하지만 Subsystem 이름을 모름... 
        // NonGameSettings에 MasterSkillTable만 있었음. ItemTable은? InventoryComponent가 들고 있을 확률 높음.

        // InventoryComponent 코드를 다시 봐야 확실하지만, 
        // 보통 ItemId만 있으면 Row를 찾을 수 있어야 함.

        // *가정*: UInventoryItem 내부 로직이나, InventoryComponent의 헬퍼 사용.
        // 만약 없다면.. 일단 넘어감(비주얼 안나옴). 
        // -> 유저가 "비주얼 안나옴" 하면 그때 고치자? 안됨. "장착 아이템 사라짐" == 비주얼 안나옴 일수도.
        
        // *시도*: NewItem->SetRow(...) ? 
        // InventoryComponent를 통해 Row를 가져오는 헬퍼 추가가 필요해 보임.
        // 하지만 지금은 UInventoryComponent 헤더를 못 수정하므로(최소 변경 원칙), 
        // EquipInternal 호출 전 Row 채우기 시도.

        // UInventoryItem 생명주기: NewObject -> ItemId 설정 -> (Row 로드) -> 사용
        // EquipInternal 호출하면 끝.
        
        // EquipInternal 내부에서 ApplyVisual 호출 시 Row를 씀.
        // 따라서 Row가 비어있으면 안됨.
        
        // 일단 인벤토리 컴포넌트 메서드 확인: AddItem 등에서 Row를 찾아서 넣어줌.
        // 여기서는 우리가 직접 만들어야 함.
        
        // UInventoryComponent에 GetItemData(ItemId) 같은게 없으면 낭패.
        // -> FindComponentByClass<UInventoryComponent>() -> GetItemData(Data.ItemId) ?
        
        // 다행히 EquipInternal(NewItem, Slot) 호출하면 됨.
        // 단, CanEquip에서 Row 검사함.
        
        // [결론] Row 채우는 코드가 필요함.
        // UInventoryComponent 코드를 본 기억에 의하면 DataTable 레퍼런스가 있었음.
        // 일단 시도: OwnerInventory->GetItemData(ItemId) (존재한다고 가정하거나, 새로 만들어야 함)
        // -> 이미 빌드된 코드라 함수 없을 확률 99%.
        
        // 대안: SaveGameSubsystem에서 복구 할 때, ItemTable에 접근 권한이 있을 수 있음.
        // 하지만 여기서 처리하는게 깔끔.
        
        // *우회*: EquipInternal 호출 후, ApplyVisual이 있는데, 여기서 Row를 씀.
        
        // [해결책] "TryGetItemRow" 함수가 InventoryComponent에 있다고 믿거나 추가해야 함.
        // -> InventoryComponent.cpp를 보면 FindRow 하는 로직이 어딘가에 있음.
        // -> AddItem(ItemId) -> ... -> FindRow.
        
        // 그렇다면, 잠시 인벤토리에 AddItem 했다가 -> 그걸 집어서 EquipFromInventory -> 다시 인벤토리에서 제거? 
        // 약간 비효율적이지만 가장 안전함 (Row 로드 로직 재사용).
        
        if (InvComp)
        {
            int32 AddedIdx = INDEX_NONE;
            // 인벤토리에 잠시 추가 (Row 로드됨)
            if (InvComp->AddItem(Data.ItemId, Data.Quantity, AddedIdx))
            {
                // 바로 장착 (EquipFromInventory 사용)
                EquipFromInventory(InvComp, AddedIdx, Data.Slot);
            }
        }
    }
}

void UEquipmentComponent::UpdateWeaponTags(bool bIsArmed)
{
    ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner());
    if (!Char) return;

    UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent();
    if (!ASC) return;
    if (!Char->HasAuthority()) return;

    // 1) 모든 무기 태그 제거 (일단 싹 지운다)
    FGameplayTagContainer AllWeaponTags;
    for (auto& Pair : WeaponTypeArmedTags)
    {
        AllWeaponTags.AppendTags(Pair.Value);
    }
    ASC->RemoveLooseGameplayTags(AllWeaponTags);

    // 2) 서브 무기 (방패 등) - 항상 태그 부여 (사용자 요청: 토글 없음)
    if (UInventoryItem* Sub = GetEquippedItemBySlot(EEquipmentSlot::WeaponSub))
    {
        if (const FGameplayTagContainer* FoundTags = WeaponTypeArmedTags.Find(Sub->CachedRow.WeaponType))
        {
            ASC->AddLooseGameplayTags(*FoundTags);
        }
    }

    if (!bIsArmed)
    {
        return;
    }

    // 3) 현재 장착된 메인 무기 - 무장 상태일 때만 부여 + "실제로 손에 있을 때"만
    if (UInventoryItem* Main = GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
    {
        // 실제 손에 들려있는지 확인 (Visual Socket Check)
        bool bActuallyInHand = false;
        
        // VisualComponents 맵에서 조회 (VisualSlots는 설정값임)
        if (TObjectPtr<UMeshComponent>* MeshPtr = VisualComponents.Find(EEquipmentSlot::WeaponMain))
        {
            if (UMeshComponent* Mesh = *MeshPtr)
            {
                const FName HandSocket = Char->GetHandSocketForItem(Main);
                if (Mesh->GetAttachSocketName() == HandSocket)
                {
                    bActuallyInHand = true;
                }
            }
        }

        // bIsArmed가 true라도, 메인 무기가 '집(등/허리)'에 붙어있으면 태그 안 줌
        if (bActuallyInHand)
        {
            if (const FGameplayTagContainer* FoundTags = WeaponTypeArmedTags.Find(Main->CachedRow.WeaponType))
            {
                ASC->AddLooseGameplayTags(*FoundTags);
            }
        }
    }
}
