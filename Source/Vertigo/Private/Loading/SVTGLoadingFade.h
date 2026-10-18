#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "Loading/VTGLevelLoadingInfo.h"
#include "Loading/VTGLoadingTransitionTiming.h"

// MoviePlayer ticks this on its Slate thread. No World/UObject access is permitted here.
class SVTGLoadingFade : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVTGLoadingFade) : _Duration(1.f), _InitialBlackHold(1.5f) {}
    SLATE_ARGUMENT(float, Duration)
    SLATE_ARGUMENT(float, InitialBlackHold)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Envelope.OpeningFade=FMath::Max(0.f,Args._Duration);
        Envelope.InitialHold=FMath::Max(0.f,Args._InitialBlackHold);
        SetCanTick(true);
        ChildSlot[SNew(SOverlay)
            +SOverlay::Slot()[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)]
            +SOverlay::Slot()[SAssignNew(ContentLayer,SBox)[Args._Content.Widget]]];
        ContentLayer->SetRenderOpacity(0.f);
        SetRenderOpacity(1.f);
    }
    void BeginClose(float FadeToBlack,float BlackHold,float Reveal)
    {
        if (Envelope.ClosingAt>=0) return;
        Envelope.ClosingFade=FMath::Max(0.f,FadeToBlack);
        Envelope.BlackHold=FMath::Max(0.f,BlackHold);
        Envelope.RevealFade=FMath::Max(0.f,Reveal);
        Envelope.ClosingAt=FPlatformTime::Seconds();
    }
    // Compatibility for the news-rotation test; actual travel uses all three configured stages.
    void BeginFadeOut(float Seconds) { BeginClose(Seconds,0.f,Seconds); }
    bool CanRevealWorld() const { return Envelope.CanReveal(FPlatformTime::Seconds()); }
    void BeginWorldReveal() { if(CanRevealWorld()) Envelope.RevealingAt=FPlatformTime::Seconds(); }
    bool IsFinished() const { return Envelope.IsFinished(FPlatformTime::Seconds()); }
    bool HasStartedDrawing() const { return Envelope.StartedAt>=0; }
    // MinimumDisplayTime counts the image, not the initial black hold or the opening fade.
    double GetFirstShownTime() const { return Envelope.ImageVisibleAt(); }
    bool IsFullyVisible() const { return Envelope.IsImageVisible(FPlatformTime::Seconds()); }

    void SetNewsRotation(TArray<FVTGLoadingNews> Items,float Seconds,int32 InitialIndex,TFunction<void(const FVTGLoadingNews&)> Update)
    {
        NewsItems=MoveTemp(Items);NewsSeconds=FMath::Max(0.f,Seconds);NewsIndex=InitialIndex;
        UpdateNews=MoveTemp(Update);NewsRandom.Initialize(FMath::Rand());
    }
    void AdvanceNews(float Seconds)
    {
        if(Envelope.ClosingAt>=0 || NewsSeconds<=0 || NewsItems.Num()<2 || !UpdateNews)return;
        NewsElapsed+=Seconds;if(NewsElapsed<NewsSeconds)return;
        NewsElapsed=FMath::Fmod(NewsElapsed,NewsSeconds);
        int32 Next=NewsRandom.RandHelper(NewsItems.Num()-1);if(Next>=NewsIndex)++Next;
        NewsIndex=Next;UpdateNews(NewsItems[Next]);
    }
    virtual void Tick(const FGeometry& Geometry,double CurrentTime,float DeltaTime) override
    {
        SCompoundWidget::Tick(Geometry,CurrentTime,DeltaTime);
        const double Now=FPlatformTime::Seconds();if(Envelope.StartedAt<0)Envelope.StartedAt=Now;
        if(Envelope.IsImageVisible(Now))AdvanceNews(DeltaTime);
        ContentLayer->SetRenderOpacity(Envelope.ImageOpacity(Now));
        SetRenderOpacity(Envelope.CoverOpacity(Now));
    }
private:
    FVTGLoadingBlackoutTiming Envelope;
    TSharedPtr<SBox> ContentLayer;
    TArray<FVTGLoadingNews> NewsItems;
    TFunction<void(const FVTGLoadingNews&)> UpdateNews;
    FRandomStream NewsRandom;
    int32 NewsIndex=0;
    float NewsSeconds=0.f;
    float NewsElapsed=0.f;
};
