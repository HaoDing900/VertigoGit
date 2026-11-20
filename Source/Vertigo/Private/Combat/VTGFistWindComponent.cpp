#include "Combat/VTGFistWindComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Components/WeaponBase_ISX.h"
#include "Objects/Item.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ChildActorComponent.h"

namespace FistWind
{
bool Flag(const UObject* O,FName Name) {if(O)if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),Name))return P->GetPropertyValue_InContainer(O);return false;}
double Number(const UObject* O,FName Name)
{
    if(O)if(auto* P=FindFProperty<FNumericProperty>(O->GetClass(),Name))
    {const void* V=P->ContainerPtrToValuePtr<void>(O);return P->IsInteger()?double(P->GetSignedIntPropertyValue(V)):P->GetFloatingPointPropertyValue(V);}
    return 0;
}
bool Alive(const UObject* O) {return Number(O,TEXT("Health"))>0 && !Flag(O,TEXT("IsDead?"));}
}
UVTGFistWindComponent::UVTGFistWindComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
    PrimaryComponentTick.TickInterval=.1f;
}
void UVTGFistWindComponent::BeginPlay()
{
    Super::BeginPlay();
    WasInCombat=FistWind::Flag(GetOwner(),TEXT("InCombat"));
    SetComponentTickInterval(WasInCombat?0.f:.1f);
    // A wounded non-combat save/load should not strand the player in the injured pose forever.
    if(!WasInCombat && FistWind::Alive(GetOwner()) && FistWind::Number(GetOwner(),TEXT("Health"))<FistWind::Number(GetOwner(),TEXT("MaxHealth_Normal")))
        GetWorld()->GetTimerManager().SetTimer(RecoveryTimer,this,&UVTGFistWindComponent::RecoverHealth,RecoveryDelay,false);
}
bool UVTGFistWindComponent::WantsWind() const
{
    auto* Character=Cast<ACharacter>(GetOwner());
    if(!Character || !WindSystem || !FistWind::Alive(Character) || !FistWind::Flag(Character,TEXT("InCombat")) ||
       !FistWind::Flag(Character,TEXT("IsAttacking")) || FistWind::Flag(Character,TEXT("HasWeaponInHand")) ||
       FistWind::Number(Character,TEXT("CurrentMeleeWeapon"))!=0 || FistWind::Flag(Character,TEXT("HidePlayerNCam")) ||
       FistWind::Flag(Character,TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"))) return false;
    if(auto* Weapon=Character->FindComponentByClass<UWeaponBase_ISX>()) if(IsValid(Weapon->GetEquippedItem()))return false;
    auto* Anim=Character->GetMesh()->GetAnimInstance();
    auto* Active=Anim?Anim->GetCurrentActiveMontage():nullptr;
    if(!Active || !PunchMontages.Contains(Active) || !Anim->Montage_IsPlaying(Active))return false;
    const float Position=Anim->Montage_GetPosition(Active);
    for(const auto& Event:Active->Notifies)
        if(Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetName().Contains(TEXT("Notify_Damage")))
            if(Position>=FMath::Max(0.f,Event.GetTime()-.08f) && Position<=Event.GetTime()+Event.GetDuration()+.08f)return true;
    return false;
}
UStaticMeshComponent* UVTGFistWindComponent::FindHammerMesh() const
{
    if(!GetOwner() || !HammerMesh)return nullptr;
    TInlineComponentArray<UStaticMeshComponent*> Meshes(GetOwner());
    for(auto* Mesh:Meshes)if(Mesh && Mesh->GetFName()==TEXT("MeleeWeapon") && Mesh->GetStaticMesh()==HammerMesh && Mesh->IsVisible() && !Mesh->bHiddenInGame)return Mesh;
    return nullptr;
}
bool UVTGFistWindComponent::WantsHammerWind() const
{
    auto* Character=Cast<ACharacter>(GetOwner());
    if(!Character || !HammerWindSystem || !FistWind::Alive(Character) || !FistWind::Flag(Character,TEXT("InCombat")) ||
       !FistWind::Flag(Character,TEXT("IsAttacking")) || FistWind::Number(Character,TEXT("CurrentMeleeWeapon"))!=1 ||
       FistWind::Flag(Character,TEXT("HidePlayerNCam")) || FistWind::Flag(Character,TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")) || !FindHammerMesh())return false;
    auto* Anim=Character->GetMesh()->GetAnimInstance();auto* Montage=Anim?Anim->GetCurrentActiveMontage():nullptr;
    if(!Montage || !HammerMontages.Contains(Montage) || !Anim->Montage_IsPlaying(Montage))return false;
    const float Position=Anim->Montage_GetPosition(Montage);
    // Match the actual hit windows, with a small visual lead/tail. Long recovery poses stay clean.
    for(const auto& Event:Montage->Notifies)
        if(Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetName().Contains(TEXT("Notify_Damage")))
            if(Position>=FMath::Max(0.f,Event.GetTime()-.08f) && Position<=Event.GetTime()+Event.GetDuration()+.05f)return true;
    return false;
}
void UVTGFistWindComponent::SetHammerWindActive(bool Active)
{
    if(!Active)
    {
        if(IsValid(HammerWind)){HammerWind->DeactivateImmediate();HammerWind->SetComponentTickEnabled(false);HammerWind->DestroyComponent();}
        HammerWind=nullptr;return;
    }
    if(IsValid(HammerWind) || !HammerWindSystem || GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    auto* Mesh=FindHammerMesh();if(!Mesh)return;
    auto* Wind=NewObject<UNiagaraComponent>(GetOwner());Wind->SetAutoActivate(false);Wind->SetAutoDestroy(false);
    Wind->SetupAttachment(Mesh);Wind->SetRelativeLocation(HammerWindOffset);Wind->SetRelativeRotation(HammerWindRotation);
    Wind->SetAbsolute(false,false,true);Wind->SetWorldScale3D(FVector(HammerWindScale));
    Wind->SetAsset(HammerWindSystem);Wind->RegisterComponent();Wind->SetCastShadow(false);Wind->Activate(true);HammerWind=Wind;
}
void UVTGFistWindComponent::SetWindActive(bool Active)
{
    if(!Active)
    {
        for(UNiagaraComponent* Wind:Winds) if(IsValid(Wind)) {Wind->DeactivateImmediate();Wind->SetComponentTickEnabled(false);Wind->DestroyComponent();}
        Winds.Empty();
        return;
    }
    if(!WindSystem || GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    if(!Winds.IsEmpty())
    {
        auto* Character=Cast<ACharacter>(GetOwner());auto* Mesh=Character?Character->GetMesh():nullptr;
        if(Mesh)for(UNiagaraComponent* Wind:Winds)if(IsValid(Wind) && Wind->GetAttachParent())
        {
            auto* Parent=Wind->GetAttachParent();
            const FString Socket=Parent->GetAttachSocketName().ToString().ToLower();
            const bool Left=Socket.Contains(TEXT("_l")) || Parent->GetName().ToLower().Contains(TEXT("left"));
            const FName Hand=Left?TEXT("hand_l"):TEXT("hand_r");
            const FName SaFinger=Left?TEXT("c_middle1_l"):TEXT("c_middle1_r");
            const FName Finger=Mesh->DoesSocketExist(SaFinger)?SaFinger:(Left?TEXT("middle_01_l"):TEXT("middle_01_r"));
            FVector Forward=Parent->GetForwardVector();
            if(Mesh->DoesSocketExist(Hand) && Mesh->DoesSocketExist(Finger))
                Forward=(Mesh->GetSocketLocation(Finger)-Mesh->GetSocketLocation(Hand)).GetSafeNormal();
            if(!Forward.IsNearlyZero())Wind->SetWorldRotation(FRotationMatrix::MakeFromX(Forward).Rotator());
        }
        return;
    }
    // Follow the actual brass-knuckle transforms, including their authored hand sockets and offsets.
    TInlineComponentArray<UStaticMeshComponent*> Meshes(GetOwner());
    for(auto* Knuckle:Meshes)
        if(Knuckle && Knuckle->GetName().Contains(TEXT("BrassKnuckle")))
        {
            auto* Wind=NewObject<UNiagaraComponent>(GetOwner());
            Wind->SetAutoActivate(false);Wind->SetAutoDestroy(false);
            Wind->SetupAttachment(Knuckle);Wind->SetRelativeTransform(FTransform(FRotator::ZeroRotator,FVector::ZeroVector,FVector(WindScale)));
            Wind->SetAbsolute(false,true,false);
            Wind->SetAsset(WindSystem);Wind->RegisterComponent();
            Wind->SetCastShadow(false);Wind->Activate(true);Winds.Add(Wind);
            if(Winds.Num()==2)break;
        }
    if(!Winds.IsEmpty())SetWindActive(true);
}
void UVTGFistWindComponent::UpdateCombatState(bool InCombat)
{
    if(InCombat) GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
    else if(WasInCombat && FistWind::Alive(GetOwner()))
        GetWorld()->GetTimerManager().SetTimer(RecoveryTimer,this,&UVTGFistWindComponent::RecoverHealth,RecoveryDelay,false);
    if(!FistWind::Alive(GetOwner()))GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
    WasInCombat=InCombat;
    SetComponentTickInterval(InCombat?0.f:.1f);
}
void UVTGFistWindComponent::TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(Dt,TickType,TickFunction);
    UpdateCombatState(FistWind::Flag(GetOwner(),TEXT("InCombat")));
    SetWindActive(WantsWind());
    SetHammerWindActive(WantsHammerWind());
}
void UVTGFistWindComponent::RecoverHealth()
{
    auto* Owner=GetOwner();
    if(!Owner || FistWind::Flag(Owner,TEXT("InCombat")) || !FistWind::Alive(Owner))return;
    const double Missing=FistWind::Number(Owner,TEXT("MaxHealth_Normal"))-FistWind::Number(Owner,TEXT("Health"));
    if(Missing>0)
        if(auto* Heal=Owner->FindFunction(TEXT("UpdateHealthByItem")))
        {
            FStructOnScope Args(Heal);
            if(auto* P=FindFProperty<FNumericProperty>(Heal,TEXT("UpdateHealth")))
            {
                void* Value=P->ContainerPtrToValuePtr<void>(Args.GetStructMemory());
                if(P->IsInteger())P->SetIntPropertyValue(Value,int64(Missing));else P->SetFloatingPointPropertyValue(Value,Missing);
                Owner->ProcessEvent(Heal,Args.GetStructMemory());
            }
        }
    if(FistWind::Number(Owner,TEXT("Health"))>=FistWind::Number(Owner,TEXT("MaxHealth_Normal")))
        if(auto* Low=FindFProperty<FBoolProperty>(Owner->GetClass(),TEXT("ShowLowHealth?")))Low->SetPropertyValue_InContainer(Owner,false);
}
void UVTGFistWindComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
    SetWindActive(false);
    SetHammerWindActive(false);
    Super::EndPlay(Reason);
}

void UVTGCombatBoundaryLibrary::SetBoundaryStyleVisible(AActor* Boundary,bool Visible)
{
    if(!IsValid(Boundary))return;
    for(UActorComponent* C:Boundary->GetComponents())
    {
        if(auto* Mesh=Cast<UStaticMeshComponent>(C);Mesh && Mesh->ComponentHasTag(TEXT("BattleStyle")))
            {Mesh->SetVisibility(Visible);Mesh->SetHiddenInGame(!Visible);}
        if(auto* Child=Cast<UChildActorComponent>(C);Child && Child->GetName().Contains(TEXT("SquareZone")))
            if(auto* Actor=Child->GetChildActor())Actor->SetActorHiddenInGame(true);
    }
}


