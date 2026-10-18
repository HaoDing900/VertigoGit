#include "AnimNode_NarrativeJaw.h"
#include "NarrativeJawComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

void FAnimNode_NarrativeJaw::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    SourcePose.Initialize(Context);
}
void FAnimNode_NarrativeJaw::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    SourcePose.CacheBones(Context);
    JawBone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
}
void FAnimNode_NarrativeJaw::Update_AnyThread(const FAnimationUpdateContext& Context)
{
    SourcePose.Update(Context);
}
void FAnimNode_NarrativeJaw::PreUpdate(const UAnimInstance* AnimInstance)
{
    Alpha = 0;
    AActor* Owner = AnimInstance->GetOwningActor();
    auto* State = Owner ? Owner->FindComponentByClass<UNarrativeJawComponent>() : nullptr;
    if (!State) return;
    // Existing character ABPs use all three spellings. Keep their authored variables intact.
    for (const FName Name : {FName(TEXT("IsCutscene")), FName(TEXT("Is Cutscene")), FName(TEXT("IsCutScene?"))})
    {
        const auto* Cutscene = FindFProperty<FBoolProperty>(AnimInstance->GetClass(), Name);
        if (Cutscene && Cutscene->GetPropertyValue_InContainer(AnimInstance)) { State->Cancel(); return; }
    }
    FNarrativeJawPlayback Playback;
    if (Sequence && State->SamplePlayback(Sequence->GetPlayLength(), Playback))
    {
        SampleTime = Playback.Position;
        Alpha = Playback.Weight;
        Amplitude = Playback.Amplitude;
    }
}
void FAnimNode_NarrativeJaw::Evaluate_AnyThread(FPoseContext& Output)
{
    SourcePose.Evaluate(Output);
    if (Alpha <= 0 || !Sequence || !JawBone.IsValidToEvaluate(Output.Pose.GetBoneContainer())) return;
    FPoseContext JawPose(Output);
    JawPose.ResetToRefPose();
    FAnimationPoseData PoseData(JawPose);
    Sequence->GetAnimationPose(PoseData, FAnimExtractContext(SampleTime, false));
    const FCompactPoseBoneIndex Index = JawBone.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
    FTransform Target = JawPose.Pose[Index];
    if (Amplitude < 1.f)
    {
        FPoseContext ClosedPose(Output);
        ClosedPose.ResetToRefPose();
        FAnimationPoseData ClosedData(ClosedPose);
        Sequence->GetAnimationPose(ClosedData, FAnimExtractContext(0., false));
        // Reduce the animation's jaw rotation relative to its authored closed pose.
        // Keep the layer's weight separate so montage jaw tracks cannot leak back in.
        Target.SetRotation(FQuat::Slerp(ClosedPose.Pose[Index].GetRotation(), Target.GetRotation(), Amplitude).GetNormalized());
    }
    FTransform Blended;
    Blended.Blend(Output.Pose[Index], Target, Alpha);
    Output.Pose[Index] = Blended;
}
