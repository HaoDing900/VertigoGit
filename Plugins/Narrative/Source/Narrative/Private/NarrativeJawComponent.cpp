#include "NarrativeJawComponent.h"
#include "Dialogue.h"
#include "Engine/World.h"
#include "Internationalization/Text.h"

#if UE_ENABLE_ICU
THIRD_PARTY_INCLUDES_START
#include <unicode/uchar.h>
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
bool IsJawPunctuation(TCHAR Character)
{
    // C-runtime character classification depends on locale; ICU also recognizes
    // Chinese/full-width punctuation, curly quotes, ellipses and Unicode dashes.
    if (Character <= 0x7f && FChar::IsPunct(Character)) return true;
#if UE_ENABLE_ICU
    return u_ispunct(static_cast<UChar32>(Character)) != 0;
#else
    // Common dialogue punctuation on targets built without ICU.
    return (Character >= 0x2000 && Character <= 0x206f)
        || (Character >= 0x3001 && Character <= 0x3003)
        || (Character >= 0x3008 && Character <= 0x301f)
        || (Character >= 0xfe10 && Character <= 0xfe19)
        || (Character >= 0xfe30 && Character <= 0xfe52)
        || (Character >= 0xfe54 && Character <= 0xfe61)
        || (Character >= 0xff01 && Character <= 0xff0f)
        || (Character >= 0xff1a && Character <= 0xff20)
        || (Character >= 0xff3b && Character <= 0xff40)
        || (Character >= 0xff5b && Character <= 0xff65)
        || Character == 0x00a1 || Character == 0x00ab || Character == 0x00b7
        || Character == 0x00bb || Character == 0x00bf;
#endif
}
}

void UNarrativeJawComponent::Start(UDialogue* Dialogue, float Duration)
{
    Source = Dialogue;
    StartedAt = GetWorld()->GetTimeSeconds();
    Seconds = Duration;
    auto Range = [](float A, float B, float Min, float Max, FVector2D Fallback)
    {
        if (!FMath::IsFinite(A) || !FMath::IsFinite(B)) return Fallback;
        A = FMath::Clamp(A, Min, Max); B = FMath::Clamp(B, Min, Max);
        return FVector2D(FMath::Min(A, B), FMath::Max(A, B));
    };
    RateRange = Range(Dialogue->JawMinPlayRate, Dialogue->JawMaxPlayRate, 0.1f, 10.f, FVector2D(0.7f, 1.2f));
    AmplitudeRange = Range(Dialogue->JawMinAmplitude, Dialogue->JawMaxAmplitude, 0.f, 1.f, FVector2D(0.5f, 1.f));
    Random.Initialize(FMath::Rand());
    LoopStartAge = 0;
    LoopIndex = 0;
    LoopRate = Random.FRandRange(RateRange.X, RateRange.Y);
    PreviousAmplitude = Random.FRandRange(AmplitudeRange.X, AmplitudeRange.Y);
    TargetAmplitude = Random.FRandRange(AmplitudeRange.X, AmplitudeRange.Y);
}

void UNarrativeJawComponent::Stop(const UDialogue* Dialogue)
{
    if (Source.Get() == Dialogue) Cancel();
}

void UNarrativeJawComponent::Cancel()
{
    Source.Reset();
    Seconds = 0;
}

bool UNarrativeJawComponent::Sample(float& Time, float& Weight)
{
    Time = 0;
    Weight = 0;
    if (!Source.IsValid() || !Source->bEnableJawMovement || !GetWorld()) { Cancel(); return false; }
    Time = GetWorld()->GetTimeSeconds() - StartedAt;
    if (Time < 0 || Time >= Seconds) { Cancel(); return false; }
    Weight = FMath::Min(1.f, FMath::Min(Time / 0.06f, (Seconds - Time) / 0.08f));
    return true;
}

bool UNarrativeJawComponent::SamplePlayback(float SequenceLength, FNarrativeJawPlayback& Out)
{
    Out = FNarrativeJawPlayback();
    float Age = 0;
    if (!Sample(Age, Out.Weight) || !FMath::IsFinite(SequenceLength) || SequenceLength <= 0) return false;
    // Integrate complete loops rather than multiplying age by a newly randomized rate.
    // This preserves animation phase across rate changes, frame hitches and repeated mesh evaluation.
    while (Age >= LoopStartAge + SequenceLength / LoopRate)
    {
        LoopStartAge += SequenceLength / LoopRate;
        ++LoopIndex;
        LoopRate = Random.FRandRange(RateRange.X, RateRange.Y);
        PreviousAmplitude = TargetAmplitude;
        TargetAmplitude = Random.FRandRange(AmplitudeRange.X, AmplitudeRange.Y);
    }
    Out.Position = FMath::Clamp(float((Age - LoopStartAge) * LoopRate), 0.f, SequenceLength);
    Out.PlayRate = LoopRate;
    Out.LoopIndex = LoopIndex;
    const float Phase = Out.Position / SequenceLength;
    const float SmoothPhase = Phase * Phase * (3.f - 2.f * Phase);
    Out.Amplitude = FMath::Lerp(PreviousAmplitude, TargetAmplitude, SmoothPhase);
    return true;
}

int32 UNarrativeJawComponent::CountVisibleCharacters(const FString& Text)
{
    int32 Count = 0;
    for (int32 I = 0; I < Text.Len(); ++I)
    {
        if (Text[I] == '<')
        {
            const int32 End = Text.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, I + 1);
            if (End != INDEX_NONE) { I = End; continue; }
        }
        if (!FText::IsWhitespace(Text[I]) && !IsJawPunctuation(Text[I])) ++Count;
    }
    return Count;
}
