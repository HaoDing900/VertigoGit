#include "Combat/VTGCombatPresentation.h"
#include "Combat/VTGCombatComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WeaponBase_ISX.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/StructOnScope.h"
#include "UI/VTGActionHUD.h"

namespace
{
void SetFlag(AActor* Owner, FName Name, bool Value)
{
    if (auto* P = FindFProperty<FBoolProperty>(Owner->GetClass(), Name))
        P->SetPropertyValue_InContainer(Owner, Value);
}
bool Flag(AActor* Owner, FName Name)
{
    auto* P = FindFProperty<FBoolProperty>(Owner->GetClass(), Name);
    return P && P->GetPropertyValue_InContainer(Owner);
}
// Use the same equipment event as the player, including future WeaponDataMap entries.
void HolsterMelee(AActor* Owner)
{
    auto* Current = FindFProperty<FNumericProperty>(Owner->GetClass(), TEXT("CurrentMeleeWeapon"));
    if (!Current || Current->GetSignedIntPropertyValue(Current->ContainerPtrToValuePtr<void>(Owner)) == 0) return;
    if (auto* Equip = Owner->FindFunction(TEXT("EquipMeleeWeapon")))
    {
        FStructOnScope Args(Equip);
        // The enum's zero entry is the existing unarmed equipment entry.
        if (auto* Parameter = FindFProperty<FNumericProperty>(Equip, TEXT("CurrentMeleeWeapon")))
        {
            Parameter->SetIntPropertyValue(Parameter->ContainerPtrToValuePtr<void>(Args.GetStructMemory()), uint64(0));
            Owner->ProcessEvent(Equip, Args.GetStructMemory());
        }
    }
}
void StopWeaponMontages(AActor* Owner)
{
    auto* Character = Cast<ACharacter>(Owner);
    auto* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    auto* MapProperty = FindFProperty<FMapProperty>(Owner->GetClass(), TEXT("WeaponDataMap"));
    auto* DataProperty = MapProperty ? CastField<FStructProperty>(MapProperty->ValueProp) : nullptr;
    if (!Anim || !DataProperty) return;
    FScriptMapHelper Map(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(Owner));
    for (TFieldIterator<FArrayProperty> It(DataProperty->Struct); It; ++It)
    {
        if (!It->GetName().StartsWith(TEXT("ComboMontages"))) continue;
        auto* ObjectProperty = CastField<FObjectPropertyBase>(It->Inner);
        if (!ObjectProperty) continue;
        for (int32 I=0; I<Map.GetMaxIndex(); ++I)
        {
            if (!Map.IsValidIndex(I)) continue;
            FScriptArrayHelper Montages(*It, It->ContainerPtrToValuePtr<void>(Map.GetValuePtr(I)));
            for (int32 J=0; J<Montages.Num(); ++J)
                if (auto* Montage = Cast<UAnimMontage>(ObjectProperty->GetObjectPropertyValue(Montages.GetRawPtr(J))))
                    if (Anim->Montage_IsPlaying(Montage)) Anim->Montage_Stop(.1f, Montage);
        }
    }
}
}

void UVTGPlayerPresentationComponent::SetStoryCombatLock(bool Enabled)
{
    auto* Owner = GetOwner();
    if (!Owner) return;
    if (Enabled)
    {
        if (!bStoryCombatLocked)
        {
            StoryLockPreviousFlags.Reset();
            for (FName Name : {FName(TEXT("CanAttack?")), FName(TEXT("CanPunch?")), FName(TEXT("StoryMode?"))})
                StoryLockPreviousFlags.Add(Name, Flag(Owner, Name));
            bStoryCombatLocked = true;
            if (auto* Combat = Owner->FindComponentByClass<UVTGCombatComponent>())
                if (Combat->GetCombatState() == EVTGCombatState::Attacking)
                    Combat->SetCombatState(EVTGCombatState::Idle);
            StopWeaponMontages(Owner);
            if (auto* Reset = Owner->FindFunction(TEXT("ResetCombo")); Reset && Reset->ParmsSize == 0)
                Owner->ProcessEvent(Reset, nullptr);
        }
        EnforceStoryCombatLock();
        RefreshBrassKnuckles();
        if (auto* Action = Cast<UVTGActionHUD>(Hearts)) Action->RefreshBloodOverlay(false);
        if (Hearts) Hearts->SetVisibility(ESlateVisibility::Collapsed);
    }
    else if (bStoryCombatLocked)
    {
        bStoryCombatLocked = false;
        // Never revive, re-equip, force combat, or release another system's cinematic input lock.
        auto* Health = FindFProperty<FNumericProperty>(Owner->GetClass(), TEXT("Health"));
        const void* HealthValue = Health ? Health->ContainerPtrToValuePtr<void>(Owner) : nullptr;
        const bool Alive = !Flag(Owner, TEXT("IsDead?")) && (!Health ||
            (Health->IsInteger() ? double(Health->GetSignedIntPropertyValue(HealthValue)) : Health->GetFloatingPointPropertyValue(HealthValue)) > 0);
        if (Alive)
            for (const auto& Pair : StoryLockPreviousFlags)
                if (Pair.Key != TEXT("StoryMode?") || !Flag(Owner, TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")))
                    SetFlag(Owner, Pair.Key, Pair.Value);
        StoryLockPreviousFlags.Reset();
    }
}

void UVTGPlayerPresentationComponent::EnforceStoryCombatLock()
{
    auto* Owner = GetOwner();
    if (!Owner) return;
    HolsterMelee(Owner);
    TInlineComponentArray<UWeaponBase_ISX*> Weapons(Owner);
    for (auto* Weapon : Weapons)
        if (Weapon->GetEquippedItem()) Weapon->HideWeapon();
    if (auto* Timer = FindFProperty<FStructProperty>(Owner->GetClass(), TEXT("FullAutoTimer"));
        Timer && Timer->Struct == FTimerHandle::StaticStruct() && GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(*Timer->ContainerPtrToValuePtr<FTimerHandle>(Owner));
    for (FName Name : {FName(TEXT("InCombat")), FName(TEXT("GunOrFistEquipped")), FName(TEXT("HasWeaponInHand")),
                      FName(TEXT("IsAutoFiring")), FName(TEXT("IsAiming")), FName(TEXT("SaveAttack")),
                      FName(TEXT("IsInComboWindow")), FName(TEXT("CanAttack?")), FName(TEXT("CanPunch?"))})
        SetFlag(Owner, Name, false);
    SetFlag(Owner, TEXT("StoryMode?"), true);
}
