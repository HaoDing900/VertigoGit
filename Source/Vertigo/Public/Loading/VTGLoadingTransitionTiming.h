#pragma once
#include "CoreMinimal.h"

// Loading timing uses real time and rendered-frame gates, independent of world timers and pause.
struct FVTGLoadingTransitionTiming
{
    double VisibleAt = -1.0;
    double ReadyAt = -1.0;
    uint64 OpaqueFrame = MAX_uint64;
    uint64 ReadyFrame = MAX_uint64;

    bool CanTravel(uint64 Frame, bool bFullyVisible)
    {
        if (!bFullyVisible) return false;
        if (OpaqueFrame == MAX_uint64) { OpaqueFrame = Frame; return false; }
        return Frame > OpaqueFrame;
    }
    void MarkVisible(double Now) { if (VisibleAt < 0.0) VisibleAt = Now; }
    void MarkReady(double Now, uint64 Frame) { ReadyAt = Now; ReadyFrame = Frame; }
    bool CanReveal(double Now, uint64 Frame, double MinimumDisplay, double HoldAfterLoad) const
    {
        return VisibleAt >= 0.0 && ReadyAt >= 0.0 && Frame > ReadyFrame &&
               Now - VisibleAt >= MinimumDisplay && Now - ReadyAt >= HoldAfterLoad;
    }
};

/** Pure real-time envelope; Slate can evaluate it without touching a World or UObject. */
struct FVTGLoadingBlackoutTiming
{
    double StartedAt = -1.0;
    double ClosingAt = -1.0;
    double RevealingAt = -1.0;
    float InitialHold = 1.5f;
    float OpeningFade = 1.f;
    float ClosingFade = 1.5f;
    float BlackHold = 1.5f;
    float RevealFade = 1.f;

    static float Smooth(double Elapsed, float Duration)
    {
        const float T = Duration <= 0.f ? 1.f : FMath::Clamp(float(Elapsed / Duration), 0.f, 1.f);
        return T*T*(3.f-2.f*T);
    }
    double ImageVisibleAt() const { return StartedAt < 0 ? -1.0 : StartedAt + InitialHold + OpeningFade; }
    bool IsImageVisible(double Now) const { return StartedAt >= 0 && Now >= ImageVisibleAt() && ClosingAt < 0; }
    float ImageOpacity(double Now) const
    {
        if (StartedAt < 0 || Now < StartedAt + InitialHold) return 0.f;
        const float Open = Smooth(Now-StartedAt-InitialHold, OpeningFade);
        return ClosingAt < 0 ? Open : Open*(1.f-Smooth(Now-ClosingAt,ClosingFade));
    }
    bool CanReveal(double Now) const { return ClosingAt >= 0 && Now >= ClosingAt + ClosingFade + BlackHold && RevealingAt < 0; }
    float CoverOpacity(double Now) const { return RevealingAt < 0 ? 1.f : 1.f-Smooth(Now-RevealingAt,RevealFade); }
    bool IsFinished(double Now) const { return RevealingAt >= 0 && Now >= RevealingAt + RevealFade; }
};

/** A transient ready signal must remain true across frames before showing the new world. */
struct FVTGLoadingReadinessTiming
{
    double StableSince = -1.0;
    uint64 FirstFrame = MAX_uint64;
    void Reset() { StableSince=-1.0; FirstFrame=MAX_uint64; }
    bool Update(bool bReady,double Now,uint64 Frame)
    {
        if (!bReady) { Reset(); return false; }
        if (StableSince < 0) { StableSince=Now; FirstFrame=Frame; }
        return Frame > FirstFrame && Now-StableSince >= .25;
    }
};
