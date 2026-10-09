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
    PrimaryComponentTick.TickInterval = .5f;
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
    auto *C = Cast<ACharacter>(GetOwner());
    // One listener owns the master reverb. Keep authored AudioVolumes authoritative.
    const bool Wanted = C && C == UGameplayStatics::GetPlayerPawn(this, 0) && C->IsLocallyControlled() &&
                        UGameplayStatics::GetCurrentLevelName(this, true) == SewerLevel.ToString();
    if (!Wanted)
    {
        if (bStarted)
            StopSewerAudio();
        return;
    }
    if (!bStarted)
    {
        bStarted = true;
        for (int32 I = 0; I < 2; ++I)
            if (auto *Audio =
                    UGameplayStatics::CreateSound2D(this, I == 0 ? WaterBed : PipeBed, 1, 1, 0, nullptr, false, false))
            {
                Audio->bIsUISound = false;
                Beds.Add(Audio);
                Audio->FadeIn(3, I == 0 ? WaterVolume : PipeVolume);
            }
        NextHissTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(12.f, 22.f);
    }
    auto *Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
    const FVector Listener = Camera ? Camera->GetCameraLocation() : C->GetPawnViewLocation();
    FReverbSettings Authored;
    auto *Volume = GetWorld()->GetAudioSettings(Listener, &Authored, nullptr);
    if (Volume && Authored.bApplyReverb && (Authored.ReverbEffect || Authored.ReverbPluginEffect))
    {
        if (bOwnReverb)
            UGameplayStatics::DeactivateReverbEffect(this, ReverbTag);
        bOwnReverb = false;
        ActiveSpace = -1;
        PendingSpace = -1;
        StableSamples = 0;
    }
    else
    {
        const int32 Space = ProbeSpace(Listener);
        StableSamples = Space == PendingSpace ? StableSamples + 1 : 1;
        PendingSpace = Space;
        if (StableSamples >= 3 && Space != ActiveSpace)
        {
            ActiveSpace = Space;
            auto *Effect = Space == 1   ? PipeReverb.Get()
                           : Space == 2 ? CorridorReverb.Get()
                           : Space == 3 ? ChamberReverb.Get()
                                        : nullptr;
            if (Effect)
            {
                UGameplayStatics::ActivateReverbEffect(this, Effect, ReverbTag, 0,
                                                       Space == 1   ? PipeWetLevel
                                                       : Space == 2 ? CorridorWetLevel
                                                                    : ChamberWetLevel,
                                                       1.8f);
                bOwnReverb = true;
            }
            else if (bOwnReverb)
            {
                UGameplayStatics::DeactivateReverbEffect(this, ReverbTag);
                bOwnReverb = false;
            }
        }
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextHissTime)
    {
        NextHissTime = Now + FMath::FRandRange(18.f, 32.f);
        // Place occasional pipe noise on a real nearby wall, not on the player's head.
        const FVector Direction = FRotator(0, FMath::FRandRange(0.f, 360.f), 0).Vector();
        FHitResult Wall;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(VTGPipeSound), true, C);
        if (PipeHiss &&
            GetWorld()->LineTraceSingleByObjectType(Wall, Listener, Listener + Direction * 1000,
                                                    FCollisionObjectQueryParams(ECC_WorldStatic), Q) &&
            Wall.Distance > 150)
        {
            if (Hiss)
                Hiss->Stop();
            Hiss = UGameplayStatics::SpawnSoundAtLocation(this, PipeHiss, Wall.ImpactPoint + Wall.ImpactNormal * 15,
                                                          FRotator::ZeroRotator, .12f, FMath::FRandRange(.85f, 1), 0,
                                                          AmbientAttenuation, nullptr, true);
        }
    }
}
void UVTGPlayerAudioComponent::StopSewerAudio()
{
    for (UAudioComponent *Audio : Beds)
        if (IsValid(Audio))
        {
            Audio->Stop();
            Audio->DestroyComponent();
        }
    Beds.Empty();
    if (IsValid(Hiss))
    {
        Hiss->Stop();
        Hiss = nullptr;
    }
    if (bOwnReverb)
        UGameplayStatics::DeactivateReverbEffect(this, ReverbTag);
    bOwnReverb = false;
    bStarted = false;
    ActiveSpace = -1;
    PendingSpace = -1;
    StableSamples = 0;
}
void UVTGPlayerAudioComponent::EndPlay(EEndPlayReason::Type Reason)
{
    StopSewerAudio();
    Super::EndPlay(Reason);
}
void UVTGFootstepNotify::Notify(USkeletalMeshComponent *Mesh, UAnimSequenceBase *, const FAnimNotifyEventReference &)
{
    if (Mesh && Mesh->GetOwner())
        if (auto *Audio = Mesh->GetOwner()->FindComponentByClass<UVTGPlayerAudioComponent>())
            Audio->PlayFootstep(FootBone);
}
