#include "System/NonGameInstance.h"

#include "System/NonGameInstance.h"

FString UNonGameInstance::GetSaveSlotName(int32 SlotIndex) const
{
    FString SlotName = FString::Printf(TEXT("Slot%d"), SlotIndex);

#if WITH_EDITOR
    // 에디터 PIE(Play In Editor) 실행 중이면, 클라이언트별로 슬롯을 분리함
    if (GIsEditor)
    {
        // WorldContext를 통해 PIE 인스턴스 번호 확인 (0, 1, 2...)
        if (const FWorldContext* CurrentContext = GetWorldContext())
        {
            // PIEInstance: 서버=0(Ded) or 리슨서버, 클라1=1, 클라2=2 ... 등등 (모드마다 다름)
            // PlayAsClient(NetMode=Client)일 때: 
            // - EditorWorld는 보통 PIEInstance가 할당됨.
            int32 PIEIndex = CurrentContext->PIEInstance;
            
            // -1이면 PIE가 아님 (일반 에디터 월드)
            if (PIEIndex != -1)
            {
                // 예: "PIE_1_Slot0"
                return FString::Printf(TEXT("PIE_%d_%s"), PIEIndex, *SlotName);
            }
        }
    }
#endif

    return SlotName;
}
