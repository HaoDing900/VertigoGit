#include "Animation/VTGCameraLookAnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Combat/VTGFistWindComponent.h"

namespace
{
bool CameraLookFlag(const UObject *O, FName Name)
{
    auto *P = FindFProperty<FBoolProperty>(O->GetClass(), Name);
    return P && P->GetPropertyValue_InContainer(O);
}
UObject* CameraLookObject(const UObject* Owner, FName Name)
{
    auto* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name);
    return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
}
bool UsingPortableTerminal(const APawn* Pawn, const UAnimInstance* Anim)
{
    if (CameraLookFlag(Pawn, TEXT("IsReadingMail"))) return true;
    // The existing terminal component stays visible until the closing animation finishes.
    if (const auto* Terminal = Cast<USceneComponent>(CameraLookObject(Pawn, TEXT("Terminal")));
        Terminal && Terminal->IsVisible()) return true;
    // Opening starts before the prop is shown; closing can outlast the reading flag.
    for (FName Name : {FName(TEXT("Mont_OpenTerminal")), FName(TEXT("Mont_ReadTerminalLoop")),
                      FName(TEXT("Mont_CloseTerminal"))})
        if (auto* Montage = Cast<UAnimMontage>(CameraLookObject(Pawn, Name));
            Montage && Anim->Montage_IsActive(Montage)) return true;
    return false;
}
} // namespace
void UVTGCameraLookAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    SmoothedLook = CameraNeckOffset = CameraHeadOffset = FRotator::ZeroRotator;
    HammerSpineAlpha = 0.f;
    PunchDownAlpha=0.f;PunchDownRotation=FRotator::ZeroRotator;
    HammerSpineRotation = FRotator(0,0,HammerSpineXDegrees);
}
FRotator UVTGCameraLookAnimInstance::ClampCameraLook(const FQuat &Body, const FRotator &View, float YawLimit,
                                                     float PitchLimit)
{
    const FRotator Relative = Body.UnrotateVector(View.Vector()).Rotation().GetNormalized();
    // Fade back to neutral when the camera faces behind the body instead of flipping at +/-180 degrees.
    const float Facing =
        1.f - FMath::GetMappedRangeValueClamped(FVector2D(95, 145), FVector2D(0, 1), FMath::Abs(Relative.Yaw));
    return FRotator(FMath::Clamp(Relative.Pitch, -PitchLimit, PitchLimit) * Facing,
                    FMath::Clamp(Relative.Yaw, -YawLimit, YawLimit) * Facing, 0);
}
void UVTGCameraLookAnimInstance::NativeUpdateAnimation(float Dt)
{
    Super::NativeUpdateAnimation(Dt);
    auto *Pawn = TryGetPawnOwner();
    auto *Mesh = GetSkelMeshComponent();
    HammerSpineRotation = FRotator(0,0,HammerSpineXDegrees);
    // Reuse the existing final bone controller for every combat stance, independent of weapon type.
    // Keep serialized HammerSpine property names so ABP_Sa's existing pins remain compatible.
    HammerSpineAlpha = bHammerSpineCorrectionEnabled && Mesh && Pawn &&
        CameraLookFlag(Pawn,TEXT("InCombat")) && !CameraLookFlag(Pawn,TEXT("StoryMode?")) &&
        !CameraLookFlag(Pawn,TEXT("IsDead?")) && !CameraLookFlag(Pawn,TEXT("HidePlayerNCam")) &&
        !CameraLookFlag(Pawn,TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")) &&
        !CameraLookFlag(this,TEXT("IsCutscene")) ? 1.f : 0.f;
    bool Punching=false;
    if(Pawn && Mesh && HammerSpineAlpha>0 && CameraLookFlag(Pawn,TEXT("IsAttacking")) && !CameraLookFlag(Pawn,TEXT("HasWeaponInHand")))
        if(auto* Weapon=FindFProperty<FByteProperty>(Pawn->GetClass(),TEXT("CurrentMeleeWeapon"));Weapon && Weapon->GetPropertyValue_InContainer(Pawn)==0)
            if(auto* Fist=Pawn->FindComponentByClass<UVTGFistWindComponent>())
                if(auto* Active=GetCurrentActiveMontage())Punching=Fist->PunchMontages.Contains(Active) && Montage_IsPlaying(Active);
    PunchDownAlpha=HammerSpineAlpha>0?FMath::FInterpTo(PunchDownAlpha,Punching?1.f:0.f,FMath::Max(0.f,Dt),20.f):0.f;
    if(Pawn && Mesh)
    {
        const FQuat BodyInMesh=Mesh->GetComponentQuat().Inverse()*Pawn->GetActorQuat();
        PunchDownRotation=(BodyInMesh*FRotator(-PunchDownDegrees,0,0).Quaternion()*BodyInMesh.Inverse()).Rotator();
    }
    else PunchDownRotation=FRotator::ZeroRotator;
    auto *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    FRotator Desired = FRotator::ZeroRotator;
    const bool Allowed = bCameraLookEnabled && Pawn && Mesh && PC && PC->IsLocalController() &&
                         PC->PlayerCameraManager && PC->GetViewTarget() == Pawn && !Pawn->IsHidden() &&
                         !CameraLookFlag(Pawn, TEXT("IsAttacking")) && !CameraLookFlag(Pawn, TEXT("IsDodging?")) &&
                         !CameraLookFlag(Pawn, TEXT("IsDead?")) && !CameraLookFlag(Pawn, TEXT("HidePlayerNCam")) &&
                         !CameraLookFlag(Pawn, TEXT("bIsLooking")) &&
                         !CameraLookFlag(Pawn, TEXT("FixedCameraLevel?")) && !UsingPortableTerminal(Pawn, this);
    if (Allowed)
        Desired = ClampCameraLook(Pawn->GetActorQuat(), PC->PlayerCameraManager->GetCameraRotation(), MaxLookYaw,
                                  MaxLookPitch);
    const float Alpha = 1.f - FMath::Exp(-FMath::Max(.1f, LookInterpSpeed) * FMath::Max(0.f, Dt));
    SmoothedLook.Pitch = FMath::Lerp(SmoothedLook.Pitch, Desired.Pitch, Alpha);
    SmoothedLook.Yaw = FMath::Lerp(SmoothedLook.Yaw, Desired.Yaw, Alpha);
    if (!Pawn || !Mesh)
    {
        CameraNeckOffset = CameraHeadOffset = FRotator::ZeroRotator;
        return;
    }
    // Bone controllers use mesh component space; Sa's imported mesh axes differ from the actor axes.
    const FQuat BodyInMesh = Mesh->GetComponentQuat().Inverse() * Pawn->GetActorQuat();
    auto Offset = [&](float Weight)
    {
        const FQuat Local = FQuat::Slerp(FQuat::Identity, SmoothedLook.Quaternion(), Weight);
        return (BodyInMesh * Local * BodyInMesh.Inverse()).Rotator();
    };
    CameraNeckOffset = Offset(.35f);
    CameraHeadOffset = Offset(.65f);
}
