#include "VTGSewerFeelAuditCommandlet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/DataTable.h"

namespace VTGSewerFeelAudit
{
const TCHAR* FistCombo[] =
{
    TEXT("/Game/Characters/Sa/Anm/FistFight/ANM_Sa_FightFist09_Inplace_Montage.ANM_Sa_FightFist09_Inplace_Montage"),
    TEXT("/Game/Characters/Sa/Anm/FistFight/ANM_Sa_FistFight0.ANM_Sa_FistFight0"),
    TEXT("/Game/Characters/Sa/Anm/FistFight/ANM_Sa_FightFist03_Inplace1_Montage.ANM_Sa_FightFist03_Inplace1_Montage"),
};
const TCHAR* HammerCombo[] =
{
    TEXT("/Game/Characters/Sa/Anm/Sa_Hammer/Combo/Hammer_Attack_2Combo_1_Root_Montage.Hammer_Attack_2Combo_1_Root_Montage"),
    TEXT("/Game/Characters/Sa/Anm/Sa_Hammer/Combo/Hammer_Attack_2Combo_2_Root_Montage.Hammer_Attack_2Combo_2_Root_Montage"),
    TEXT("/Game/Characters/Sa/Anm/Sa_Hammer/Combo/Hammer_Attack_Infinite_1_Montage.Hammer_Attack_Infinite_1_Montage"),
};
const TCHAR* Dodges[] =
{
    TEXT("/Game/Characters/Sa/Anm/Dodge/ANM_Sa_Dodge_F2_Montage.ANM_Sa_Dodge_F2_Montage"),
    TEXT("/Game/Characters/Sa/Anm/Dodge/ANM_Sa_Dodge_B_Montage.ANM_Sa_Dodge_B_Montage"),
    TEXT("/Game/Characters/Sa/Anm/Dodge/ANM_Sa_Dodge_L_Montage.ANM_Sa_Dodge_L_Montage"),
    TEXT("/Game/Characters/Sa/Anm/Dodge/ANM_Sa_Dodge_R_Montage.ANM_Sa_Dodge_R_Montage"),
};
const TCHAR* BossMontages[] =
{
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_Headbutt.AM_EggProducer_Headbutt"),
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_ToxicGas.AM_EggProducer_ToxicGas"),
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_AcidSpit.AM_EggProducer_AcidSpit"),
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_SpawnEggs.AM_EggProducer_SpawnEggs"),
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_GotHit.AM_EggProducer_GotHit"),
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_SeaMonsterEgg_ReleaseGas.AM_SeaMonsterEgg_ReleaseGas"),
    TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_SeaMonsterEgg_GotHit.AM_SeaMonsterEgg_GotHit"),
};

void Dump(const TCHAR* Label, const TCHAR* Path)
{
    auto* Montage = LoadObject<UAnimMontage>(nullptr, Path);
    if (!Montage)
    {
        UE_LOG(LogTemp, Warning, TEXT("FEEL %s: MISSING %s"), Label, Path);
        return;
    }
    const float Length = Montage->GetPlayLength();
    UE_LOG(LogTemp, Display, TEXT("FEEL %s | %s | length=%.3fs rate=%.2f blendIn=%.3f blendOut=%.3f"),
        Label, *Montage->GetName(), Length, Montage->RateScale,
        Montage->BlendIn.GetBlendTime(), Montage->BlendOut.GetBlendTime());
    // A notify's trigger time is what the player actually feels: input to impact, and when the next input is accepted.
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        const UObject* Payload = Event.NotifyStateClass ? static_cast<UObject*>(Event.NotifyStateClass) : static_cast<UObject*>(Event.Notify);
        const float Start = Event.GetTriggerTime();
        const float Duration = Event.GetDuration();
        if (Event.NotifyStateClass)
        {
            UE_LOG(LogTemp, Display, TEXT("FEEL     state  %-34s %6.3f -> %6.3f  (%.0f%% - %.0f%% of montage)"),
                *GetNameSafe(Payload), Start, Start + Duration,
                Length > 0 ? Start / Length * 100 : 0, Length > 0 ? (Start + Duration) / Length * 100 : 0);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("FEEL     notify %-34s %6.3f             (%.0f%% of montage) name=%s"),
                *GetNameSafe(Payload), Start, Length > 0 ? Start / Length * 100 : 0, *Event.NotifyName.ToString());
        }
    }
}

int32 Audit()
{
    UE_LOG(LogTemp, Display, TEXT("FEEL ==== player fist combo (damage 10 / 20 / 30) ===="));
    for (int32 I = 0; I < UE_ARRAY_COUNT(FistCombo); ++I) Dump(*FString::Printf(TEXT("fist%d"), I + 1), FistCombo[I]);
    UE_LOG(LogTemp, Display, TEXT("FEEL ==== player hammer combo (damage 40 / 50 / 60) ===="));
    for (int32 I = 0; I < UE_ARRAY_COUNT(HammerCombo); ++I) Dump(*FString::Printf(TEXT("hammer%d"), I + 1), HammerCombo[I]);
    UE_LOG(LogTemp, Display, TEXT("FEEL ==== dodge (IsDodging? is true for the whole montage) ===="));
    for (const TCHAR* Path : Dodges) Dump(TEXT("dodge"), Path);
    UE_LOG(LogTemp, Display, TEXT("FEEL ==== boss / egg moves ===="));
    for (const TCHAR* Path : BossMontages) Dump(TEXT("boss"), Path);

    if (auto* Phases = LoadObject<UDataTable>(nullptr, TEXT("/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases.DT_SeaMonster_Phases")))
    {
        UE_LOG(LogTemp, Display, TEXT("FEEL ==== DT_SeaMonster_Phases ====\n%s"), *Phases->GetTableAsString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FEEL: DT_SeaMonster_Phases missing"));
    }
    return 0;
}
}

UVTGSewerFeelAuditCommandlet::UVTGSewerFeelAuditCommandlet() { IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UVTGSewerFeelAuditCommandlet::Main(const FString&) { return VTGSewerFeelAudit::Audit(); }
