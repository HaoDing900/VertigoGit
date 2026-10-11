#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Loading/VTGLevelLoadingInfo.h"

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

    bool HasStartedDrawing() const { return StartedAt >= 0.0; }
    double GetFirstShownTime() const { return StartedAt; }
    bool IsFullyVisible() const { return !bFadingOut && HasStartedDrawing() && GetRenderOpacity() >= 0.999f; }

    // Prepared on the game thread before playback. The callback owns Slate widgets, never UObjects.
    void SetNewsRotation(TArray<FVTGLoadingNews> Items, float Seconds, int32 InitialIndex,
                         TFunction<void(const FVTGLoadingNews &)> Update)
    {
        NewsItems = MoveTemp(Items);
        NewsSeconds = FMath::Max(0.f, Seconds);
        NewsIndex = InitialIndex;
        UpdateNews = MoveTemp(Update);
        NewsRandom.Initialize(FMath::Rand());
    }

    void AdvanceNews(float Seconds)
    {
        if (bFadingOut || NewsSeconds <= 0.f || NewsItems.Num() < 2 || !UpdateNews)
            return;
        NewsElapsed += Seconds;
        if (NewsElapsed < NewsSeconds)
            return;
        NewsElapsed = FMath::Fmod(NewsElapsed, NewsSeconds);
        // Uniformly choose any other entry, avoiding consecutive repeats.
        int32 Next = NewsRandom.RandHelper(NewsItems.Num() - 1);
        if (Next >= NewsIndex) ++Next;
        NewsIndex = Next;
        UpdateNews(NewsItems[NewsIndex]);
    }

    virtual void Tick(const FGeometry &Geometry, double CurrentTime, float DeltaTime) override
    {
        SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
        AdvanceNews(DeltaTime);
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
    TArray<FVTGLoadingNews> NewsItems;
    TFunction<void(const FVTGLoadingNews &)> UpdateNews;
    FRandomStream NewsRandom;
    int32 NewsIndex = 0;
    float NewsSeconds = 0.f;
    float NewsElapsed = 0.f;
};
