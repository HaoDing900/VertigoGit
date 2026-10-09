#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// Slate ticks on MoviePlayer's loading thread. Never access UObjects from this widget's Tick.
class SVTGLoadingFade : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SVTGLoadingFade) : _Duration(0.4f)
    {
    }
    SLATE_ARGUMENT(float, Duration)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(const FArguments &Args)
    {
        Duration = FMath::Max(0.0f, Args._Duration);
        SetCanTick(true);
        ChildSlot[Args._Content.Widget];
        SetRenderOpacity(Duration > 0.0f ? 0.0f : 1.0f);
    }

    void BeginFadeOut(float Seconds)
    {
        bFadingOut = true;
        Duration = FMath::Max(0.0f, Seconds);
        StartedAt = -1.0;
        bLoggedHalf = false;
        SetRenderOpacity(Duration > 0.0f ? 1.0f : 0.0f);
    }

    bool IsFinished() const
    {
        return bFadingOut && StartedAt >= 0.0 && FPlatformTime::Seconds() - StartedAt >= Duration;
    }

    virtual void Tick(const FGeometry &Geometry, double CurrentTime, float DeltaTime) override
    {
        SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
        // Start on the first rendered frame, not during synchronous font/class loading.
        if (StartedAt < 0.0)
            StartedAt = FPlatformTime::Seconds();
        const float T = Duration <= 0.0f
                            ? 1.0f
                            : FMath::Clamp(float((FPlatformTime::Seconds() - StartedAt) / Duration), 0.0f, 1.0f);
        const float Smooth = T * T * (3.0f - 2.0f * T);
        SetRenderOpacity(bFadingOut ? 1.0f - Smooth : Smooth);
        if (!bLoggedHalf && T >= 0.5f)
        {
            bLoggedHalf = true;
            UE_LOG(LogTemp, Verbose, TEXT("VTG loading fade %s: opacity %.3f"), bFadingOut ? TEXT("out") : TEXT("in"),
                   GetRenderOpacity());
        }
    }

  private:
    double StartedAt = -1.0;
    float Duration = 0.4f;
    bool bFadingOut = false;
    bool bLoggedHalf = false;
};
