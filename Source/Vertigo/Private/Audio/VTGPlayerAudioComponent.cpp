#include "Audio/VTGPlayerAudioComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSync.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
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
bool UVTGPlayerAudioComponent::CanPlayFootstep(bool AllowStationary) const
{
    const auto *C = Cast<ACharacter>(GetOwner());
    if (!C || !GetWorld() || !GetWorld()->IsGameWorld() || C->IsHidden() ||
        !C->GetCharacterMovement()->IsMovingOnGround() || (!AllowStationary && C->GetVelocity().Size2D() < 12.f))
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
float UVTGPlayerAudioComponent::GetFootstepContextVolumeMultiplier() const
{
    const auto* C=Cast<ACharacter>(GetOwner());
    auto* Anim=C && C->GetMesh()?C->GetMesh()->GetAnimInstance():nullptr;
    if(!C || !Flag(C,TEXT("NoRunning!SlowDownWalk")) || !Anim)return 1.f;
    auto Restricted=[](const FAnimTickRecord& Record)
    {
        return Record.SourceAsset && Record.EffectiveBlendWeight>KINDA_SMALL_NUMBER &&
            Record.SourceAsset->GetPathName()==TEXT("/Game/Characters/Sa/Anm/BS_Sa_NoRunning.BS_Sa_NoRunning");
    };
    // The read buffer is the same evaluated animation frame that produced the Notify.
    for(const auto& Record:Anim->GetUngroupedActivePlayersRead())if(Restricted(Record))return .8f;
    for(const auto& Group:Anim->GetSyncGroupMapRead())for(const auto& Record:Group.Value.ActivePlayers)if(Restricted(Record))return .8f;
    return 1.f;
}
bool UVTGPlayerAudioComponent::PlayFootstep(FName FootBone,bool Landing)
{
    if (!CanPlayFootstep(Landing) || GetWorld()->GetNetMode() == NM_DedicatedServer ||
        GetWorld()->GetTimeSeconds() - LastStepTime < .16f)
        return false;
    auto *C = CastChecked<ACharacter>(GetOwner());
    FVector Foot = C->GetMesh()->DoesSocketExist(FootBone)
                       ? C->GetMesh()->GetSocketLocation(FootBone)
                       : C->GetActorLocation() - FVector(0, 0, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Q(SCENE_QUERY_STAT(VTGFootstep), true, C);
    Q.bReturnPhysicalMaterial = true;
    Q.bReturnFaceIndex = true;
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0, 0, 35), Foot - FVector(0, 0, 120), ECC_Visibility,
                                              Q))
        return false;
    const auto *Variants = &ResolveFootstepPool(Hit);
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
                                          FootstepVolume * Strength * GetFootstepContextVolumeMultiplier(), FMath::FRandRange(.94f, 1.06f), 0, StepAttenuation,
                                          nullptr, C);
    return true;
}
const TArray<TObjectPtr<USoundBase>> &UVTGPlayerAudioComponent::ResolveFootstepPool(const FHitResult &Hit) const
{
    const auto *C = Cast<ACharacter>(GetOwner());
    if (C && C->GetMesh() && BarefootMesh && C->GetMesh()->GetSkeletalMeshAsset() == BarefootMesh)
        return BarefootSteps;
    const auto *Surface = Hit.GetComponent();
    if(const auto* Mesh=Cast<UStaticMeshComponent>(Surface))
        if(Mesh->GetStaticMesh() && (Mesh->GetStaticMesh()->GetPathName()==TEXT("/Game/EnvArt/ModernCity/Meshes/Props/SM_FireEscape_Landing2.SM_FireEscape_Landing2") ||
            Mesh->GetStaticMesh()->GetPathName()==TEXT("/Game/EnvArt/PortCity/Meshes/SM_Slums_ArchitecturalKit/SM_Stairs_Metal.SM_Stairs_Metal") ||
            Mesh->GetStaticMesh()->GetPathName()==TEXT("/Game/EnvArt/PortCity/Meshes/SM_Slums_ArchitecturalKit/SM_Stairs_Small_Metal.SM_Stairs_Small_Metal")) && !MetalSteps.IsEmpty())
            return MetalSteps;
    auto Tagged = [&](FName Tag) {
        return (Surface && Surface->ComponentHasTag(Tag)) || (Hit.GetActor() && Hit.GetActor()->ActorHasTag(Tag));
    };
    const TArray<TObjectPtr<USoundBase>> *Pool = nullptr;
    if (Tagged(TEXT("AudioSurface.Water"))) Pool = &WaterSteps;
    else if (Tagged(TEXT("AudioSurface.Metal"))) Pool = &MetalSteps;
    else if (Tagged(TEXT("AudioSurface.Concrete"))) Pool = &ConcreteSteps;
    else if (Tagged(TEXT("AudioSurface.Wood"))) Pool = &WoodSteps;
    UMaterialInterface *Material = nullptr;
    if (Surface)
    {
        int32 Section = INDEX_NONE;
        if (Hit.FaceIndex != INDEX_NONE)
            Material = Surface->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section);
        // Simple collision has no face: only accept an unambiguous material.
        if (!Material && Surface->GetNumMaterials() > 0)
        {
            Material = Surface->GetMaterial(0);
            for (int32 I = 1; I < Surface->GetNumMaterials(); ++I)
                if (Surface->GetMaterial(I) != Material) { Material = nullptr; break; }
        }
    }
    static const TMap<FString, bool> MaterialIsMetal = {
        {TEXT("/Game/Cyberpunk_Gigapack/Materials/Material_Instance/MI_BarFloorTiles6.MI_BarFloorTiles6"), false},
        {TEXT("/Game/EnvArt/ModernCity/Materials/MI_L4Street1.MI_L4Street1"), false},
        {TEXT("/Game/EnvArt/ModernCity/Materials/MI_L2Street.MI_L2Street"), false},
        {TEXT("/Game/EnvArt/ModernCity/Materials/MI_FireEscape_a.MI_FireEscape_a"), true},
        {TEXT("/Game/EnvArt/ModernCity/Materials/MI_B4_Trim_b.MI_B4_Trim_b"), true},
        {TEXT("/Game/EnvArt/CyberpunkRestaurant/Materials/MI_Ground_Tiles_01.MI_Ground_Tiles_01"), true},
        {TEXT("/Game/EnvArt/CyberpunkRestaurant/Materials/MI_Ground_Concrete.MI_Ground_Concrete"), false},
        {TEXT("/Game/EnvArt/Material/Hard/M_Wall_Concrete1.M_Wall_Concrete1"), false},
        {TEXT("/Game/EnvArt/Material/Hard/M_FloorConcrete.M_FloorConcrete"), false},
        {TEXT("/Game/EnvArt/Material/Hard/M_Concrete_CastInSitu.M_Concrete_CastInSitu"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Sci-fi/M__ScifiPipe.M__ScifiPipe"), true},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1_Inst.M_Wall_Concrete1_Inst"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1.M_Wall_Concrete1"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Uni_Concrete_CastInSituConstant.M_Uni_Concrete_CastInSituConstant"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Uni_Concrete_CastInSitu1.M_Uni_Concrete_CastInSitu1"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Uni_Concrete_CastInSitu.M_Uni_Concrete_CastInSitu"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_FloorConcrete.M_FloorConcrete"), false},
        {TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Concrete_CastInSitu.M_Concrete_CastInSitu"), false},
        {TEXT("/Game/EnvArt/ModularSciFi/Materials/PaintedMetal/MI_PaintedMetal2Indoor.MI_PaintedMetal2Indoor"), true},
        {TEXT("/Game/EnvArt/ModularSciFi/Materials/Grid/MI_Grid2.MI_Grid2"), true},
        {TEXT("/Game/Cyberpunk_Gigapack/Materials/Material_Instance/MI_Ground_Tiles_01.MI_Ground_Tiles_01"), true},
        {TEXT("/Game/Cyberpunk_Gigapack/Materials/Material_Instance/MI_Ground_Concrete.MI_Ground_Concrete"), false},
        {TEXT("/Game/Cyberpunk_Gigapack/Materials/Material_Instance/MI_CoffeeShop_ConcreteRoof.MI_CoffeeShop_ConcreteRoof"), false},
    };
    for (const UMaterialInterface *Parent = Material; !Pool && Parent;)
    {
        if (const bool *Metal = MaterialIsMetal.Find(Parent->GetPathName()))
            Pool = *Metal ? &MetalSteps : &ConcreteSteps;
        const auto *Instance = Cast<UMaterialInstance>(Parent);
        Parent = Instance ? Instance->Parent.Get() : nullptr;
    }
    if (!Pool)
    {
        const FString Names = (GetNameSafe(Material) + TEXT(" ") + GetNameSafe(Hit.PhysMaterial.Get())).ToLower();
        if (Names.Contains(TEXT("water"))) Pool = &WaterSteps;
        else if (Names.Contains(TEXT("metal")) || Names.Contains(TEXT("pipe")) || Names.Contains(TEXT("grate")) || Names.Contains(TEXT("steel"))) Pool = &MetalSteps;
        else if (Names.Contains(TEXT("wood")) || (Hit.PhysMaterial.IsValid() && Hit.PhysMaterial->SurfaceType == SurfaceType1)) Pool = &WoodSteps;
        else if (Names.Contains(TEXT("concrete")) || (Hit.PhysMaterial.IsValid() && Hit.PhysMaterial->SurfaceType == SurfaceType2)) Pool = &ConcreteSteps;
    }
    return Pool && !Pool->IsEmpty() ? *Pool : DefaultSteps;
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
            Audio->PlayFootstep(FootBone,bLanding);
}
