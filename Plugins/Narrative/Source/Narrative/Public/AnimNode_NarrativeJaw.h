#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/BoneReference.h"
#include "AnimNode_NarrativeJaw.generated.h"

class UAnimSequence;

/** Replaces only the local jaw transform. All other bones, curves and attributes pass through untouched. */
USTRUCT(BlueprintInternalUseOnly)
struct NARRATIVE_API FAnimNode_NarrativeJaw : public FAnimNode_Base
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links) FPoseLink SourcePose;
    UPROPERTY(EditAnywhere, Category = Settings) TObjectPtr<UAnimSequence> Sequence;
    UPROPERTY(EditAnywhere, Category = Settings) FBoneReference JawBone;

    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate_AnyThread(FPoseContext& Output) override;
    virtual bool HasPreUpdate() const override { return true; }
    virtual void PreUpdate(const UAnimInstance* AnimInstance) override;
private:
    float SampleTime = 0;
    float Alpha = 0;
    float Amplitude = 1;
};
