#include "VTGNarrativeInsert.h"
#include "VTGClinicFix.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StructOnScope.h"

namespace
{
const FName InsertTag(TEXT("VTG.Narrative.Insert.Active"));
float PlayClip(UAnimInstance* Anim, UAnimMontage* Clip, float Rate, bool Instant)
{
    if (!Instant) return Anim->Montage_Play(Clip, Rate);
    FMontageBlendSettings Blend;
    Blend.Blend = FAlphaBlendArgs(0.f);
    return Anim->Montage_PlayWithBlendSettings(Clip, Blend, Rate);
}
void RefreshPose(ACharacter* Character)
{
    auto* Mesh = Character->GetMesh();
    Mesh->TickAnimation(0.f, false);
    Mesh->RefreshBoneTransforms();
}
void TrackPlayer(UObject* Context,ACharacter* Character,UAnimMontage* Montage)
{
    if(!Character || !Character->IsA(LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"))))return;
    auto* Cleanup=Character->FindComponentByClass<UVTGNarrativeMontageCleanup>();
    if(!Cleanup){Cleanup=NewObject<UVTGNarrativeMontageCleanup>(Character);Character->AddInstanceComponent(Cleanup);Cleanup->RegisterComponent();}
    Cleanup->Track(Context,Character->GetMesh()->GetAnimInstance(),Montage);
}
}

bool UVTGNarrativeInsertState::Play(ACharacter* Character, UAnimMontage* Clip, bool Instant)
{
    UAnimInstance* Current = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Current || !Clip) return false;
    if (Anim && Anim != Current) Finish(false);
    if (!Insert)
    {
        Anim = Current;
        Base = Anim->GetCurrentActiveMontage();
        BaseNextSections.Reset();
        if (auto* Instance = Base ? Anim->GetActiveInstanceForMontage(Base) : nullptr)
        {
            BasePosition = Instance->GetPosition();
            BaseRate = Instance->GetPlayRate();
            BasePlaying = Instance->IsPlaying();
            for (int32 I=0; I<Base->CompositeSections.Num(); ++I) BaseNextSections.Add(Instance->GetNextSectionID(I));
        }
        Anim->OnMontageEnded.AddUniqueDynamic(this, &UVTGNarrativeInsertState::OnEnded);
    }
    // Montage_Play can synchronously interrupt the previous insert, including when restarting the same asset.
    TGuardValue<bool> Guard(Replacing, true);
    const float Result = PlayClip(Anim, Clip, 1.f, Instant);
    if (Result <= 0.f)
    {
        if (!Insert || !Anim->Montage_IsActive(Insert)) Finish(true);
        return false;
    }
    Insert = Clip;
    Character->Tags.AddUnique(InsertTag);
    if (Instant) RefreshPose(Character);
    return true;
}

void UVTGNarrativeInsertState::OnEnded(UAnimMontage* Montage, bool Interrupted)
{
    // Old blend-out callbacks may arrive after a replacement has already started.
    if (Replacing || Montage != Insert || (Anim && Anim->Montage_IsActive(Insert))) return;
    Finish(!Interrupted);
}

void UVTGNarrativeInsertState::Finish(bool Restore)
{
    if (Anim) Anim->OnMontageEnded.RemoveDynamic(this, &UVTGNarrativeInsertState::OnEnded);
    if (GetOwner()) GetOwner()->Tags.Remove(InsertTag);
    Insert = nullptr;
    auto* Character = Cast<ACharacter>(GetOwner());
    if (Restore && Anim && Base && Character && Character->GetMesh()->GetAnimInstance() == Anim
        && !Anim->GetCurrentActiveMontage())
    {
        if (Anim->Montage_Play(Base, BaseRate, EMontagePlayReturnType::MontageLength, BasePosition, false) > 0.f)
        {
            for (int32 I=0; I<BaseNextSections.Num(); ++I)
                Anim->Montage_SetNextSection(Base->GetSectionName(I), BaseNextSections[I] == INDEX_NONE ? NAME_None : Base->GetSectionName(BaseNextSections[I]), Base);
            if (!BasePlaying) Anim->Montage_Pause(Base);
            if(auto* Cleanup=Character->FindComponentByClass<UVTGNarrativeMontageCleanup>())Cleanup->TrackRestored(Anim,Base);
        }
    }
    Base = nullptr;
    Anim = nullptr;
    BaseNextSections.Reset();
}

void UVTGNarrativeInsertState::EndPlay(const EEndPlayReason::Type Reason)
{
    Finish(false);
    Super::EndPlay(Reason);
}

void UVTGNarrativeInsertLibrary::PlayNarrativeInsert(UObject* WorldContextObject, FName TargetIdentity, UAnimMontage* InterruptMontage, bool Instant)
{
    if (!InterruptMontage) return;
    TArray<AActor*> Targets;
    if (TargetIdentity == TEXT("SaP"))
    {
        // Preserve the event's original SaP lookup, including an unpossessed gameplay pawn.
        auto* PlayerClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));
        if (PlayerClass) if (auto* Player = UGameplayStatics::GetActorOfClass(WorldContextObject, PlayerClass)) Targets.Add(Player);
    }
    else UGameplayStatics::GetAllActorsWithTag(WorldContextObject, TargetIdentity, Targets);
    for (auto* Actor : Targets)
        if (auto* Character = Cast<ACharacter>(Actor))
        {
            auto* State = Character->FindComponentByClass<UVTGNarrativeInsertState>();
            if (!State)
            {
                State = NewObject<UVTGNarrativeInsertState>(Character);
                Character->AddInstanceComponent(State);
                State->RegisterComponent();
            }
            if(State->Play(Character, InterruptMontage, Instant))TrackPlayer(WorldContextObject,Character,InterruptMontage);
        }
}

float UVTGNarrativeInsertLibrary::PlayNarrativeMontage(ACharacter* Character, UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName, bool Instant, UObject* WorldContextObject)
{
    if (!IsValid(Character)) return 0.f;
    if (!Instant)
    {
        const float Result=Character->PlayAnimMontage(AnimMontage, InPlayRate, StartSectionName);
        if(Result>0.f)TrackPlayer(WorldContextObject,Character,AnimMontage);
        return Result;
    }
    auto* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Anim || !AnimMontage) return 0.f;
    const float Result = PlayClip(Anim, AnimMontage, InPlayRate, true);
    if (Result > 0.f)
    {
        if (!StartSectionName.IsNone()) Anim->Montage_JumpToSection(StartSectionName, AnimMontage);
        RefreshPose(Character);
        TrackPlayer(WorldContextObject,Character,AnimMontage);
    }
    return Result;
}

void UVTGNarrativeInsertLibrary::RefreshNarrativePose(ACharacter* Character)
{
    if(!Character || !Character->GetMesh())return;
    auto* Anim=Character->GetMesh()->GetAnimInstance();if(!Anim)return;
    // The yard's authored graph clears this flag after the idle setter. Do it before evaluating
    // the new pose as well, otherwise the refresh still evaluates the outgoing cinematic branch.
    if(auto* Function=Anim->FindFunction(TEXT("SetIsCutscene")))
    {
        FStructOnScope Args(Function);
        Anim->ProcessEvent(Function,Args.GetStructMemory());
    }
    RefreshPose(Character);
}
