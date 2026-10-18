#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeJawComponent.generated.h"

class UDialogue;

/** Sampled on the game thread. Multiple evaluations at the same world time return the same result. */
struct FNarrativeJawPlayback
{
    float Position = 0;
    float Weight = 0;
    float PlayRate = 1;
    float Amplitude = 1;
    int32 LoopIndex = 0;
};

/** Per-scene-actor jaw playback state; no timers or montage slots are used. */
UCLASS(Transient)
class NARRATIVE_API UNarrativeJawComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    void Start(UDialogue* Dialogue, float Duration);
    void Stop(const UDialogue* Dialogue);
    void Cancel();
    bool Sample(float& Time, float& Weight);
    bool SamplePlayback(float SequenceLength, FNarrativeJawPlayback& Out);
    static int32 CountVisibleCharacters(const FString& Text);
private:
    TWeakObjectPtr<UDialogue> Source;
    double StartedAt = 0;
    float Seconds = 0;
    FRandomStream Random;
    FVector2D RateRange = FVector2D(0.7f, 1.2f);
    FVector2D AmplitudeRange = FVector2D(0.5f, 1.f);
    double LoopStartAge = 0;
    float LoopRate = 1;
    float PreviousAmplitude = 1;
    float TargetAmplitude = 1;
    int32 LoopIndex = 0;
};
