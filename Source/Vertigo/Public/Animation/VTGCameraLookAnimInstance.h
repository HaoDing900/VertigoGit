#pragma once
#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "VTGCameraLookAnimInstance.generated.h"

/** Supplies camera look offsets to the existing Sa animation graph. Reads gameplay state only on the game thread. */
UCLASS(Blueprintable)
class VERTIGO_API UVTGCameraLookAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Look") bool bCameraLookEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Look", meta = (ClampMin = "0", ClampMax = "80"))
    float MaxLookYaw = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Look", meta = (ClampMin = "0", ClampMax = "45"))
    float MaxLookPitch = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Look", meta = (ClampMin = "0.1"))
    float LookInterpSpeed = 7;
    UPROPERTY(BlueprintReadOnly, Transient, Category = "Camera Look") FRotator CameraNeckOffset = FRotator::ZeroRotator;
    UPROPERTY(BlueprintReadOnly, Transient, Category = "Camera Look") FRotator CameraHeadOffset = FRotator::ZeroRotator;
    // Serialized member names retained for existing AnimGraph connections and Blueprint defaults.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Posture", meta=(DisplayName="Combat Spine Correction Enabled")) bool bHammerSpineCorrectionEnabled = true;
    /** Additive bone-local X rotation (Unreal Roll), shared by fists and all weapons during combat. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Posture", meta=(DisplayName="Combat Spine X Degrees")) float HammerSpineXDegrees = -15.f;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Combat Posture", meta=(DisplayName="Combat Spine Rotation")) FRotator HammerSpineRotation = FRotator::ZeroRotator;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Combat Posture", meta=(DisplayName="Combat Spine Alpha")) float HammerSpineAlpha = 0.f;
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    static FRotator ClampCameraLook(const FQuat &Body, const FRotator &View, float YawLimit, float PitchLimit);

  private:
    FRotator SmoothedLook = FRotator::ZeroRotator;
};
