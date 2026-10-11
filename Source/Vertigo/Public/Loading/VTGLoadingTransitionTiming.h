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
