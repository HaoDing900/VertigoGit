#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/VTGLevelLoadingInfo.h"
#include "Loading/VTGMediaLoadingPageSettings.h"
#include "MoviePlayer.h"
#include "SVTGLoadingFade.h"
#include "Loading/VTGLoadingTransitionTiming.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StreamableRenderAsset.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Save/VTGSaveCoordinator.h"


void UVTGMediaLoadingPageSystem::CacheLoadingAssets()
{
    const auto* Settings = GetDefault<UVTGMediaLoadingPageSettings>();
    CachedLoadingWidgetClass = Settings->OverlayWidgetClass.LoadSynchronous();
    CachedLoadingInfo = Settings->LevelLoadingInfo.LoadSynchronous();
}
void UVTGMediaLoadingPageSystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    // Prepare UI dependencies during GameInstance startup, rather than while a map swap is blocking.
    if (FApp::CanEverRender()) CacheLoadingAssets();
    if (IsMoviePlayerEnabled())
        PlaybackFinishedHandle = GetMoviePlayer()->OnMoviePlaybackFinished().AddUObject(
            this, &UVTGMediaLoadingPageSystem::HandlePlaybackFinished);

    PostLoadMapHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVTGMediaLoadingPageSystem::HandlePostLoadMap);
    if (GEngine)
        TravelFailureHandle =
            GEngine->OnTravelFailure().AddWeakLambda(this,
                                                     [this](UWorld *World, ETravelFailure::Type, const FString &)
                                                     {
                                                         if (World && World->GetGameInstance() == GetGameInstance())
                                                         {
                                                             CancelLoadingTransition();
                                                             ClearLoadingTransition();
                                                         }
                                                     });
    // Fires at the start of every map change (OpenLevel / travel) - the correct moment to arm the
    // MoviePlayer, before the engine flushes and blocks on the new level.
    PreLoadMapHandle =
        FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UVTGMediaLoadingPageSystem::HandlePreLoadMap);
}

void UVTGMediaLoadingPageSystem::Deinitialize()
{
    FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    if (GEngine)
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    CancelLoadingTransition();
    if (IsMoviePlayerEnabled())
        GetMoviePlayer()->OnMoviePlaybackFinished().Remove(PlaybackFinishedHandle);
    ClearPreview();
    ClearLoadingTransition();
    Super::Deinitialize();
}

void UVTGMediaLoadingPageSystem::HandlePreLoadMap(const FString &MapName)
{
    DestinationLevel = MapName;
    ClearPreview();
    if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
    {
        if (!bTransitionActive) StartViewportLoadingPage();
        return;
    }
    if (!bMovieArmed) SetupLoadingPage();
    if (bLoadingInViewport && LoadingFade && FadeViewport.IsValid())
    {
        FadeViewport->RemoveViewportWidgetContent(LoadingFade.ToSharedRef());
        bLoadingInViewport = false;
    }
    // A raw OpenLevel can reach this delegate after MoviePlayer's own PreLoadMap callback.
    if (bMovieArmed && IsMoviePlayerEnabled() && !GetMoviePlayer()->IsMovieCurrentlyPlaying())
        bStartedMovieManually = GetMoviePlayer()->PlayMovie();
}

void UVTGMediaLoadingPageSystem::ArmLoadingPage()
{
    SetupLoadingPage();
}

void UVTGMediaLoadingPageSystem::OpenLevelWithLoadingPage(FName LevelName, bool bAbsolute, const FString &Options)
{
    if (LevelName.IsNone()) return;
    if (bTransitionActive || LoadingFade) return;
    DestinationLevel = LevelName.ToString();
    if (StartViewportLoadingPage())
    {
        PendingLevel = LevelName;
        PendingOptions = Options;
        bPendingAbsolute = bAbsolute;
        return;
    }
    // Disabled/headless/video-only fallback. PreLoadMap still arms MoviePlayer when appropriate.
    UGameplayStatics::OpenLevel(this, LevelName, bAbsolute, Options);
}

void UVTGMediaLoadingPageSystem::SetupLoadingPage()
{
    if (!bRuntimeEnabled || !FApp::CanEverRender())
    {
        return;
    }

    const UVTGMediaLoadingPageSettings *Settings = GetDefault<UVTGMediaLoadingPageSettings>();
    if (!Settings || !Settings->bEnabled)
    {
        return;
    }

    // Nothing to show? Don't register an empty loading page (would just be a black frame).
    const bool bHasOverlay = !Settings->OverlayWidgetClass.IsNull();
    if (Settings->MovieNames.Num() == 0 && !bHasOverlay)
    {
        return;
    }

    if (!IsMoviePlayerEnabled())
    {
        return;
    }

    const bool bReuseVisibleScreen = bTransitionActive && LoadingFade.IsValid();
    if (!bReuseVisibleScreen) ClearLoadingTransition();
    FLoadingScreenAttributes Attributes;
    Attributes.MinimumLoadingScreenDisplayTime = bReuseVisibleScreen
        ? FMath::Max(0.f,Settings->MinimumDisplayTime-float(FPlatformTime::Seconds()-Timing.VisibleAt))
        : Settings->InitialBlackHoldDuration + Settings->FadeInDuration + Settings->MinimumDisplayTime;
    // Dismiss once the new map is ready (but never before MinimumLoadingScreenDisplayTime).
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bMoviesAreSkippable = Settings->bSkippable;
    // MT_LoadingLoop loops the last clip until loading finishes - hides variable load time.
    Attributes.PlaybackType = Settings->bLoopUntilLoaded ? MT_LoadingLoop : MT_Normal;
    Attributes.MoviePaths = Settings->MovieNames;

    // Optional UMG overlay composited over the movie (vignette / title / grain).
    if (bReuseVisibleScreen)
        Attributes.WidgetLoadingScreen = LoadingFade;
    else if (bHasOverlay)
    {
        if (UClass *WidgetClass = CachedLoadingWidgetClass.Get())
        {
            if (UGameInstance *GI = GetGameInstance())
            {
                if (UUserWidget *Widget = CreateWidget<UUserWidget>(GI, WidgetClass))
                {
                    ActiveLoadingWidget = Widget;
                    Attributes.WidgetLoadingScreen = Widget->TakeWidget();
                    ApplyLevelContent(Widget, DestinationLevel);
                }
            }
        }
    }

    if (!Attributes.WidgetLoadingScreen.IsValid() && Attributes.MoviePaths.IsEmpty())
    {
        Attributes.WidgetLoadingScreen = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
        UE_LOG(LogTemp, Warning, TEXT("Loading page widget could not be created; using fallback."));
    }
    if (Attributes.WidgetLoadingScreen.IsValid() && !bReuseVisibleScreen)
    {
        LoadingFade =
            SNew(SVTGLoadingFade).Duration(Settings->FadeInDuration).InitialBlackHold(Settings->InitialBlackHoldDuration)[Attributes.WidgetLoadingScreen.ToSharedRef()];
        Attributes.WidgetLoadingScreen = LoadingFade;
        ConfigureNewsRotation(ActiveLoadingWidget, DestinationLevel, LoadingFade);
    }
    if (!bTransitionActive && LoadingFade)
    {
        bTransitionActive = true;
        Timing = FVTGLoadingTransitionTiming();
        TransitionTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVTGMediaLoadingPageSystem::TickLoadingTransition));
    }
    bMovieFinished = false;
    bMovieArmed = true;
    GetMoviePlayer()->SetupLoadingScreen(Attributes);
    UE_LOG(LogTemp, Display, TEXT("VTG loading page armed: %s"), *GetNameSafe(ActiveLoadingWidget));
}

void UVTGMediaLoadingPageSystem::PreviewLoadingPage(float Duration, FName LevelName)
{
    ClearPreview();
    UWorld *World = GetWorld();
    if (!World || !FApp::CanEverRender())
        return;
    UClass *WidgetClass = CachedLoadingWidgetClass.Get();
    if (!WidgetClass)
        return;
    PreviewWidget = CreateWidget<UUserWidget>(GetGameInstance(), WidgetClass);
    if (!PreviewWidget)
        return;
    PreviewWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    UGameViewportClient *Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport)
    {
        ClearPreview();
        return;
    }
    PreviewFade =
        SNew(SVTGLoadingFade)
            .Duration(GetDefault<UVTGMediaLoadingPageSettings>()->FadeInDuration)
            .InitialBlackHold(GetDefault<UVTGMediaLoadingPageSettings>()->InitialBlackHoldDuration)[PreviewWidget->TakeWidget()];
    ApplyLevelContent(PreviewWidget, LevelName.IsNone() ? World->GetPackage()->GetName() : LevelName.ToString());
    ConfigureNewsRotation(PreviewWidget, LevelName.IsNone() ? World->GetPackage()->GetName() : LevelName.ToString(), PreviewFade);
    PreviewViewport = Viewport;
    Viewport->AddViewportWidgetContent(PreviewFade.ToSharedRef(), 10000);
    World->GetTimerManager().SetTimer(PreviewTimer, this, &UVTGMediaLoadingPageSystem::HideLoadingPagePreview,
                                      GetDefault<UVTGMediaLoadingPageSettings>()->InitialBlackHoldDuration + GetDefault<UVTGMediaLoadingPageSettings>()->FadeInDuration + FMath::Max(Duration, 0.1f), false);
}

void UVTGMediaLoadingPageSystem::HideLoadingPagePreview()
{
    if (UWorld *World = GetWorld())
        World->GetTimerManager().ClearTimer(PreviewTimer);
    if (!PreviewFade || PreviewFadeTicker.IsValid())
        return;
    const auto* Settings=GetDefault<UVTGMediaLoadingPageSettings>();
    PreviewFade->BeginClose(Settings->FadeToBlackDuration,Settings->HoldAfterLoadDuration,Settings->FadeOutDuration);
    PreviewFadeTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UVTGMediaLoadingPageSystem::TickPreviewFadeOut));
}

void UVTGMediaLoadingPageSystem::ClearPreview()
{
    if (UWorld *World = GetWorld())
        World->GetTimerManager().ClearTimer(PreviewTimer);
    FTSTicker::GetCoreTicker().RemoveTicker(PreviewFadeTicker);
    PreviewFadeTicker.Reset();
    if (PreviewViewport.IsValid() && PreviewFade)
        PreviewViewport->RemoveViewportWidgetContent(PreviewFade.ToSharedRef());
    PreviewViewport.Reset();
    PreviewFade.Reset();
    PreviewWidget = nullptr;
}

bool UVTGMediaLoadingPageSystem::TickPreviewFadeOut(float DeltaTime)
{
    if (PreviewFade && PreviewFade->CanRevealWorld()) PreviewFade->BeginWorldReveal();
    if (PreviewFade && !PreviewFade->IsFinished())
        return true;
    PreviewFadeTicker.Reset();
    ClearPreview();
    return false;
}

static FAutoConsoleCommandWithWorldAndArgs GVTGPreviewLoading(
    TEXT("VTG.PreviewLoading"),
    TEXT("Preview loading text for a destination: VTG.PreviewLoading L2Bar (PIE supported)."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args, UWorld *World)
        {
            if (World && World->GetGameInstance())
                World->GetGameInstance()->GetSubsystem<UVTGMediaLoadingPageSystem>()->PreviewLoadingPage(
                    5.0f, Args.IsEmpty() ? NAME_None : FName(*Args[0]));
        }));

void UVTGMediaLoadingPageSystem::HandlePlaybackFinished()
{
    if (!bTransitionActive || !LoadingFade) return;
    // MoviePlayer has released its Slate thread. Keep the identical screen opaque while the new world settles.
    bMovieFinished = true;
    bMovieArmed = false;
    bStartedMovieManually = false;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (Viewport && !bLoadingInViewport)
    {
        FadeViewport = Viewport;
        Viewport->AddViewportWidgetContent(LoadingFade.ToSharedRef(), 10000);
        bLoadingInViewport = true;
    }
}
bool UVTGMediaLoadingPageSystem::TickFadeOut(float DeltaTime)
{
    const double Now=FPlatformTime::Seconds();
    const bool bReady=Readiness.Update(IsDestinationReadyToReveal(),Now,GFrameCounter);
    if (!bReady && Now>=NextReadinessLogAt)
    {
        FString Reason;IsDestinationReadyToReveal(&Reason);NextReadinessLogAt=Now+5.0;
        UE_LOG(LogTemp,Display,TEXT("VTG loading holding black for: %s"),Reason.IsEmpty()?TEXT("readiness stability frames"):*Reason);
    }
    if (LoadingFade && LoadingFade->CanRevealWorld())
    {
        if (BlackCoverFrame==MAX_uint64) BlackCoverFrame=GFrameCounter;
        if (bReady && GFrameCounter>BlackCoverFrame)
        {
            LoadingFade->BeginWorldReveal();
            UE_LOG(LogTemp,Display,TEXT("VTG loading black hold finished and destination settled; revealing world."));
        }
    }
    if (LoadingFade && !LoadingFade->IsFinished())
        return true;
    FadeTicker.Reset();
    ClearLoadingTransition();
    UE_LOG(LogTemp, Display, TEXT("VTG loading fade-out finished; overlay removed."));
    return false;
}

void UVTGMediaLoadingPageSystem::ClearLoadingTransition()
{
    FTSTicker::GetCoreTicker().RemoveTicker(FadeTicker);
    FadeTicker.Reset();
    if (FadeViewport.IsValid() && LoadingFade)
        FadeViewport->RemoveViewportWidgetContent(LoadingFade.ToSharedRef());
    FadeViewport.Reset();
    LoadingFade.Reset();
    DestinationWorld.Reset();
    Readiness.Reset();
    BlackCoverFrame=MAX_uint64;
    bLoadingInViewport = false;
    ActiveLoadingWidget = nullptr;
}

void UVTGMediaLoadingPageSystem::OpenLevelWithLoadingScreen(const UObject *WorldContextObject,
                                                            TSoftObjectPtr<UWorld> Level, bool bAbsolute,
                                                            FString Options)
{
    if (UGameInstance *GI = UGameplayStatics::GetGameInstance(WorldContextObject))
        GI->GetSubsystem<UVTGMediaLoadingPageSystem>()->OpenLevelWithLoadingPage(
            FName(*Level.ToSoftObjectPath().GetLongPackageName()), bAbsolute, Options);
}

void UVTGMediaLoadingPageSystem::OpenLevelWithLoadingScreenByName(const UObject *WorldContextObject,
                                                                 FName LevelName, bool bAbsolute, FString Options)
{
    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
        GI->GetSubsystem<UVTGMediaLoadingPageSystem>()->OpenLevelWithLoadingPage(LevelName, bAbsolute, Options);
    else
        UGameplayStatics::OpenLevel(WorldContextObject, LevelName, bAbsolute, Options);
}

bool UVTGMediaLoadingPageSystem::StartViewportLoadingPage()
{
    UWorld* World = GetWorld();
    const auto *Settings = GetDefault<UVTGMediaLoadingPageSettings>();
    UGameViewportClient *Viewport = GetGameInstance()->GetGameViewportClient();
    if (!World || !bRuntimeEnabled || !Settings->bEnabled || !Viewport || !FApp::CanEverRender())
        return false;
    UClass *WidgetClass = CachedLoadingWidgetClass.Get();
    if (!WidgetClass)
        return false;
    ClearPreview();
    ClearLoadingTransition();
    ActiveLoadingWidget = CreateWidget<UUserWidget>(GetGameInstance(), WidgetClass);
    if (!ActiveLoadingWidget)
        return false;
    LoadingFade = SNew(SVTGLoadingFade).Duration(Settings->FadeInDuration).InitialBlackHold(Settings->InitialBlackHoldDuration)[ActiveLoadingWidget->TakeWidget()];
    ApplyLevelContent(ActiveLoadingWidget, DestinationLevel);
    ConfigureNewsRotation(ActiveLoadingWidget, DestinationLevel, LoadingFade);
    FadeViewport = Viewport;
    Viewport->AddViewportWidgetContent(LoadingFade.ToSharedRef(), 10000);
    bTransitionActive = true;
    bMapReady = false;
    bLoadingInViewport = true;
    bMovieFinished = World->WorldType == EWorldType::PIE || !IsMoviePlayerEnabled();
    bMovieArmed = false;
    bStartedMovieManually = false;
    Timing = FVTGLoadingTransitionTiming();
    TransitionTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UVTGMediaLoadingPageSystem::TickLoadingTransition));
    UE_LOG(LogTemp, Display, TEXT("VTG loading screen shown in the old world before travel."));
    return true;
}

void UVTGMediaLoadingPageSystem::HandlePostLoadMap(UWorld *World)
{
    if (!World && bTransitionActive)
    {
        CancelLoadingTransition();
        ClearLoadingTransition();
        return;
    }
    if (bTransitionActive && World && World->GetGameInstance() == GetGameInstance())
    {
        bMapReady = true;
        DestinationWorld=World;
        Readiness.Reset();
        Timing.MarkReady(FPlatformTime::Seconds(), GFrameCounter);
        if (bStartedMovieManually && IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
            GetMoviePlayer()->WaitForMovieToFinish();
        UE_LOG(LogTemp, Display, TEXT("VTG destination ready: %s"), *World->GetPackage()->GetName());
    }
}

bool UVTGMediaLoadingPageSystem::TickLoadingTransition(float DeltaTime)
{
    if (!bTransitionActive || !LoadingFade)
    {
        TransitionTicker.Reset();
        return false;
    }
    const double Now = FPlatformTime::Seconds();
    if (LoadingFade->HasStartedDrawing()) Timing.MarkVisible(LoadingFade->GetFirstShownTime());
    if (!PendingLevel.IsNone() && Timing.CanTravel(GFrameCounter, LoadingFade->IsFullyVisible()))
    {
        const FName Level = PendingLevel;
        PendingLevel = NAME_None;
        // Arm before OpenLevel: MoviePlayer's PreLoadMap callback is registered before ours.
        if (GetWorld()->WorldType != EWorldType::PIE && IsMoviePlayerEnabled()) SetupLoadingPage();
        UE_LOG(LogTemp, Display, TEXT("VTG loading fully visible before travel; opening %s"), *Level.ToString());
        UGameplayStatics::OpenLevel(this, Level, bPendingAbsolute, PendingOptions);
    }
    const auto* Settings = GetDefault<UVTGMediaLoadingPageSettings>();
    if (bMapReady && bMovieFinished && Timing.CanReveal(Now, GFrameCounter, Settings->MinimumDisplayTime, 0.0))
    {
        TransitionTicker.Reset();
        bTransitionActive = false;
        if (LoadingFade && FadeViewport.IsValid())
        {
            LoadingFade->BeginClose(Settings->FadeToBlackDuration,Settings->HoldAfterLoadDuration,Settings->FadeOutDuration);
            NextReadinessLogAt=Now+5.0;
            FadeTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVTGMediaLoadingPageSystem::TickFadeOut));
        }
        else ClearLoadingTransition();
        UE_LOG(LogTemp, Display, TEXT("VTG map loaded; image to black %.2fs, minimum black hold %.2fs, world reveal %.2fs after readiness."),Settings->FadeToBlackDuration,Settings->HoldAfterLoadDuration,Settings->FadeOutDuration);
        return false;
    }
    return true;
}

void UVTGMediaLoadingPageSystem::CancelLoadingTransition()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TransitionTicker);
    TransitionTicker.Reset();
    bTransitionActive = false;
    bMapReady = false;
    bMovieFinished = false;
    bMovieArmed = false;
    bStartedMovieManually = false;
    Timing = FVTGLoadingTransitionTiming();
    PendingLevel = NAME_None;
}

bool UVTGMediaLoadingPageSystem::IsDestinationReadyToReveal(FString* WaitingFor) const
{
    auto Wait=[WaitingFor](const FString& Reason){if(WaitingFor)*WaitingFor=Reason;return false;};
    UWorld* World=DestinationWorld.Get();
    if (!World || World!=GetWorld() || !World->AreActorsInitialized() || !World->HasBegunPlay()) return Wait(TEXT("destination actor initialization / BeginPlay"));
    if (IsAsyncLoading()) return Wait(TEXT("asynchronous package loading"));
    if (const auto* Save=GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>();Save && Save->HasPendingWorldRestore()) return Wait(TEXT("saved world restoration"));
    for (ULevelStreaming* Level:World->GetStreamingLevels())
        if (Level && (Level->IsStreamingStatePending() || (Level->ShouldBeLoaded() && !Level->IsLevelLoaded()) || (Level->ShouldBeVisible() && !Level->IsLevelVisible()))) return Wait(Level->GetPathName());
    if (auto* Partition=World->GetSubsystem<UWorldPartitionSubsystem>();Partition && !Partition->IsAllStreamingCompleted()) return Wait(TEXT("World Partition streaming"));
    // Check operations actually in flight rather than wanted mip counts, which can exceed a memory budget forever.
    if (GetDefault<UVTGMediaLoadingPageSettings>()->bWaitForRenderAssetStreaming)
        for (TObjectIterator<UStreamableRenderAsset> It;It;++It)
            if (!It->IsTemplate() && It->HasPendingInitOrStreaming()) return Wait(It->GetPathName());
    return true;
}

void UVTGMediaLoadingPageSystem::ApplyLevelContent(UUserWidget *Widget, const FString &LevelName)
{
    if (const auto *Info = CachedLoadingInfo.Get())
        Info->ApplyToWidget(Widget, LevelName);
}

void UVTGMediaLoadingPageSystem::ConfigureNewsRotation(UUserWidget *Widget, const FString &LevelName,
                                                       TSharedPtr<SVTGLoadingFade> Fade)
{
    const auto *Info = CachedLoadingInfo.Get();
    if (!Info || !Widget || !Widget->WidgetTree || !Fade) return;
    auto Items = Info->GetNewsForLevel(LevelName);
    if (Items.IsEmpty()) return;
    // Resolve and retain only Slate text widgets before MoviePlayer starts its loading thread.
    auto MakeSetter = [Widget](FName Name) -> TFunction<void(const FText &)>
    {
        auto *Target = Widget->WidgetTree->FindWidget(Name);
        if (!Target) return {};
        auto Slate = Target->GetCachedWidget();
        if (!Slate) return {};
        if (Cast<UTextBlock>(Target) && Slate->GetType() == FName(TEXT("STextBlock")))
        {
            auto Text = StaticCastSharedPtr<STextBlock>(Slate);
            return [Text](const FText &Value) { Text->SetText(Value); };
        }
        if (Cast<URichTextBlock>(Target) && Slate->GetType() == FName(TEXT("SRichTextBlock")))
        {
            auto Text = StaticCastSharedPtr<SRichTextBlock>(Slate);
            return [Text](const FText &Value) { Text->SetText(Value); };
        }
        UE_LOG(LogTemp, Warning, TEXT("Loading news rotation requires a cached text widget: %s"), *Name.ToString());
        return {};
    };
    auto Body = MakeSetter(TEXT("TipText"));
    if (!Body) return;
    int32 InitialIndex = 0;
    if (auto *Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TipText"))))
        for (int32 I = 0; I < Items.Num(); ++I)
            if (Items[I].Body.EqualTo(Text->GetText())) { InitialIndex = I; break; }
    auto Update = [Body = MoveTemp(Body)](const FVTGLoadingNews &Item)
    {
        if (Body) Body(Item.Body);
    };
    Update(Items[InitialIndex]);
    Fade->SetNewsRotation(MoveTemp(Items), Info->NewsRotationSeconds, InitialIndex, MoveTemp(Update));
}
