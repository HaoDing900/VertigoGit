#include "Animation/VTGCameraLookAnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
bool Flag(const UObject *O, FName Name)
{
    auto *P = FindFProperty<FBoolProperty>(O->GetClass(), Name);
    return P && P->GetPropertyValue_InContainer(O);
}
} // namespace
void UVTGCameraLookAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    SmoothedLook = CameraNeckOffset = CameraHeadOffset = FRotator::ZeroRotator;
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
    auto *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    FRotator Desired = FRotator::ZeroRotator;
    const bool Allowed = bCameraLookEnabled && Pawn && Mesh && PC && PC->IsLocalController() &&
                         PC->PlayerCameraManager && PC->GetViewTarget() == Pawn && !Pawn->IsHidden() &&
                         !Flag(Pawn, TEXT("IsAttacking")) && !Flag(Pawn, TEXT("IsDodging?")) &&
                         !Flag(Pawn, TEXT("IsDead?")) && !Flag(Pawn, TEXT("HidePlayerNCam")) &&
                         !Flag(Pawn, TEXT("bIsLooking"));
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
