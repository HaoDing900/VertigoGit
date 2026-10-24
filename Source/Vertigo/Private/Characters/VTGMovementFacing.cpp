#include "Characters/VTGMovementFacing.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

FRotator UVTGMovementFacing::MovementFacingRotation(ACharacter* Character, FRotator DesiredWorldRotation, double FacingYawOffset)
{
    // Follow already defines its offset as an additive world yaw. Reuse that convention.
    const FQuat World = FRotator(0, FacingYawOffset, 0).Quaternion() * DesiredWorldRotation.Quaternion();
    const auto* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
    if (Capsule && Capsule->GetAttachParent() && !Capsule->IsUsingAbsoluteRotation())
        return (Capsule->GetAttachParent()->GetComponentQuat().Inverse() * World).Rotator();
    return World.Rotator();
}
