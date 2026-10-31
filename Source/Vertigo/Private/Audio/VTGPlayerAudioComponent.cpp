#include "Audio/VTGPlayerAudioComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/AudioVolume.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/UnrealType.h"

namespace
{
const FName ReverbTag(TEXT("VTG_SewerGeometry"));
bool Flag(const UObject *O, FName Name)
{
    auto *P = FindFProperty<FBoolProperty>(O->GetClass(), Name);
    return P && P->GetPropertyValue_InContainer(O);
}
} // namespace
UVTGPlayerAudioComponent::UVTGPlayerAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}
bool UVTGPlayerAudioComponent::ConsumeArmorRunStart()
{
    const auto *C = Cast<ACharacter>(GetOwner());
    const bool Running = C && CanPlayFootstep() &&
        (Flag(C, TEXT("NoRunning!SlowDownWalk"))
             ? C->GetCharacterMovement()->MaxWalkSpeed > RestrictedRunSpeedThreshold
             : Flag(C, TEXT("IsSprinting")));
    const bool Started = Running && !bWasRunning;
    bWasRunning = Running;
    // Track running even when naked, so changing clothes mid-run is not another start.
    return Started && C->GetMesh() && C->GetMesh()->GetSkeletalMeshAsset() && BarefootMesh &&
           C->GetMesh()->GetSkeletalMeshAsset() != BarefootMesh;
}
bool UVTGPlayerAudioComponent::PlayArmorRunStart()
{
    auto *C = Cast<ACharacter>(GetOwner());
    if (!C || !CanPlayFootstep() || !C->GetMesh() || !BarefootMesh ||
        !C->GetMesh()->GetSkeletalMeshAsset() || C->GetMesh()->GetSkeletalMeshAsset() == BarefootMesh ||
        ArmorRunStartSounds.IsEmpty() || GetWorld()->GetNetMode() == NM_DedicatedServer)
        return false;
    auto *Sound = ArmorRunStartSounds[FMath::RandRange(0, ArmorRunStartSounds.Num() - 1)].Get();
    if (!Sound)
        return false;
    if (IsValid(ArmorAudio))
        ArmorAudio->Stop();
    ArmorAudio = UGameplayStatics::SpawnSoundAttached(Sound, C->GetMesh(), NAME_None, FVector::ZeroVector,
        EAttachLocation::KeepRelativeOffset, true, ArmorVolume, 1.f, 0.f, StepAttenuation, nullptr, true);
    return IsValid(ArmorAudio);
}
bool UVTGPlayerAudioComponent::CanPlayFootstep() const
{
    const auto *C = Cast<ACharacter>(GetOwner());
    if (!C || !GetWorld() || !GetWorld()->IsGameWorld() || C->IsHidden() ||
        !C->GetCharacterMovement()->IsMovingOnGround() || C->GetVelocity().Size2D() < 12.f)
        return false;
    return !Flag(C, TEXT("IsAttacking")) && !Flag(C, TEXT("IsDodging?")) && !Flag(C, TEXT("IsDead?")) &&
           !Flag(C, TEXT("HidePlayerNCam"));
}
const TArray<TObjectPtr<USoundBase>> &UVTGPlayerAudioComponent::GetOutfitFootsteps() const
{
    const auto *C = Cast<ACharacter>(GetOwner());
    // The apartment's IsSaNaked? flag lives on its level manager. Use the actual body mesh so
    // outfit swaps and restored appearances work in any level without copying that flag.
    if (C && C->GetMesh() && BarefootMesh && C->GetMesh()->GetSkeletalMeshAsset() == BarefootMesh &&
        !BarefootSteps.IsEmpty())
        return BarefootSteps;
    return DefaultSteps;
}
bool UVTGPlayerAudioComponent::PlayFootstep(FName FootBone)
{
    if (!CanPlayFootstep() || GetWorld()->GetNetMode() == NM_DedicatedServer ||
        GetWorld()->GetTimeSeconds() - LastStepTime < .16f)
        return false;
    auto *C = CastChecked<ACharacter>(GetOwner());
    FVector Foot = C->GetMesh()->DoesSocketExist(FootBone)
                       ? C->GetMesh()->GetSocketLocation(FootBone)
                       : C->GetActorLocation() - FVector(0, 0, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Q(SCENE_QUERY_STAT(VTGFootstep), true, C);
    Q.bReturnPhysicalMaterial = true;
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0, 0, 35), Foot - FVector(0, 0, 120), ECC_Visibility,
                                              Q))
        return false;
    const auto *Surface = Hit.GetComponent();
    FString Names = GetNameSafe(Hit.PhysMaterial.Get()) + TEXT(" ") + GetNameSafe(Surface) + TEXT(" ") +
                    GetNameSafe(Hit.GetActor());
    if (Surface)
        for (int32 I = 0; I < Surface->GetNumMaterials(); ++I)
            Names += TEXT(" ") + GetNameSafe(Surface->GetMaterial(I));
    Names = Names.ToLower();
    auto Tagged = [&](FName Tag)
    { return (Surface && Surface->ComponentHasTag(Tag)) || (Hit.GetActor() && Hit.GetActor()->ActorHasTag(Tag)); };
    const auto *Variants = &ConcreteSteps;
    if (Tagged(TEXT("AudioSurface.Water")) || Names.Contains(TEXT("water")))
        Variants = &WaterSteps;
    else if (Tagged(TEXT("AudioSurface.Metal")) || Names.Contains(TEXT("metal")) || Names.Contains(TEXT("pipe")) ||
             Names.Contains(TEXT("grate")) || Names.Contains(TEXT("steel")))
        Variants = &MetalSteps;
    else if (Tagged(TEXT("AudioSurface.Wood")) || Names.Contains(TEXT("wood")) ||
             (Hit.PhysMaterial.IsValid() && Hit.PhysMaterial->SurfaceType == SurfaceType1))
        Variants = &WoodSteps;
    if (Variants->IsEmpty())
        Variants = &ConcreteSteps;
    const auto &OutfitSteps = GetOutfitFootsteps();
    if (!OutfitSteps.IsEmpty())
        Variants = &OutfitSteps;
    if (Variants->IsEmpty())
        return false;
    int32 Index = FMath::RandRange(0, Variants->Num() - 1);
    if (Variants->Num() > 1 && Index == LastVariant)
        Index = (Index + 1) % Variants->Num();
    if (!(*Variants)[Index])
        return false;
    LastVariant = Index;
    LastStepTime = GetWorld()->GetTimeSeconds();
    const float Strength =
        FMath::GetMappedRangeValueClamped(FVector2D(60, 550), FVector2D(.55f, 1.15f), C->GetVelocity().Size2D());
    UGameplayStatics::PlaySoundAtLocation(this, (*Variants)[Index], Hit.ImpactPoint, FRotator::ZeroRotator,
                                          FootstepVolume * Strength, FMath::FRandRange(.94f, 1.06f), 0, StepAttenuation,
                                          nullptr, C);
    return true;
}
int32 UVTGPlayerAudioComponent::ProbeSpace(const FVector &Listener) const
{
    if (!GetWorld())
        return 0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(VTGAcoustics), true, GetOwner());
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    float Distances[8];
    int32 Walls = 0;
    for (int32 I = 0; I < 8; ++I)
    {
        const float A = I * PI / 4;
        FHitResult H;
        const bool Hit = GetWorld()->LineTraceSingleByObjectType(
            H, Listener, Listener + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 2000, Objects, Q);
        Distances[I] = Hit ? H.Distance : 2000;
        Walls += Hit;
    }
    FHitResult Ceiling;
    const bool Covered =
        GetWorld()->LineTraceSingleByObjectType(Ceiling, Listener, Listener + FVector(0, 0, 1600), Objects, Q);
    if (!Covered && Walls < 5)
        return 0;
    float Width = 4000;
    for (int32 I = 0; I < 4; ++I)
        Width = FMath::Min(Width, Distances[I] + Distances[I + 4]);
    if (Covered && Width < 500 && Ceiling.Distance < 450)
        return 1;
    if (Width < 1100 && (!Covered || Ceiling.Distance < 800))
        return 2;
    return 3;
}
void UVTGPlayerAudioComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction *Fn)
{
    Super::TickComponent(Dt, TickType, Fn);
    if (ConsumeArmorRunStart())
        PlayArmorRunStart();
    if (IsValid(ArmorAudio))
    {
        const auto *OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter && OwnerCharacter->GetMesh()->GetSkeletalMeshAsset() == BarefootMesh)
            ArmorAudio->Stop();
    }
}
void UVTGPlayerAudioComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if (IsValid(ArmorAudio))
        ArmorAudio->Stop();
    Super::EndPlay(Reason);
}
void UVTGFootstepNotify::Notify(USkeletalMeshComponent *Mesh, UAnimSequenceBase *, const FAnimNotifyEventReference &)
{
    if (Mesh && Mesh->GetOwner())
        if (auto *Audio = Mesh->GetOwner()->FindComponentByClass<UVTGPlayerAudioComponent>())
            Audio->PlayFootstep(FootBone);
}
