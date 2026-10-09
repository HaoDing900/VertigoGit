#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Blueprint/UserWidget.h"
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
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

void UVTGMediaLoadingPageSystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
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
                                                             CancelPIELoading();
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
    CancelPIELoading();
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
        if (!bPIELoading)
            StartPIELoadingPage();
        return;
    }
    SetupLoadingPage();
}

void UVTGMediaLoadingPageSystem::ArmLoadingPage()
{
    SetupLoadingPage();
}

void UVTGMediaLoadingPageSystem::OpenLevelWithLoadingPage(FName LevelName, bool bAbsolute, const FString &Options)
{
    if (bPIELoading)
        return;
    DestinationLevel = LevelName.ToString();
    if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
    {
        if (bPIELoading)
            return; // Ignore double clicks during the menu transition.
        if (StartPIELoadingPage())
        {
            PendingLevel = LevelName;
            PendingOptions = Options;
            bPendingAbsolute = bAbsolute;
            return;
        }
    }
    // Standalone MoviePlayer takes over at PreLoadMap.
    SetupLoadingPage();
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

    ClearLoadingTransition();
    FLoadingScreenAttributes Attributes;
    Attributes.MinimumLoadingScreenDisplayTime = FMath::Max(Settings->MinimumDisplayTime, Settings->FadeInDuration);
    // Dismiss once the new map is ready (but never before MinimumLoadingScreenDisplayTime).
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bMoviesAreSkippable = Settings->bSkippable;
    // MT_LoadingLoop loops the last clip until loading finishes - hides variable load time.
    Attributes.PlaybackType = Settings->bLoopUntilLoaded ? MT_LoadingLoop : MT_Normal;
    Attributes.MoviePaths = Settings->MovieNames;

    // Optional UMG overlay composited over the movie (vignette / title / grain).
    if (bHasOverlay)
    {
        if (UClass *WidgetClass = Settings->OverlayWidgetClass.LoadSynchronous())
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
    if (Attributes.WidgetLoadingScreen.IsValid())
    {
        LoadingFade =
            SNew(SVTGLoadingFade).Duration(Settings->FadeInDuration)[Attributes.WidgetLoadingScreen.ToSharedRef()];
        Attributes.WidgetLoadingScreen = LoadingFade;
    }
    GetMoviePlayer()->SetupLoadingScreen(Attributes);
    UE_LOG(LogTemp, Display, TEXT("VTG loading page armed: %s"), *GetNameSafe(ActiveLoadingWidget));
}

void UVTGMediaLoadingPageSystem::PreviewLoadingPage(float Duration, FName LevelName)
{
    ClearPreview();
    UWorld *World = GetWorld();
    if (!World || !FApp::CanEverRender())
        return;
    UClass *WidgetClass = GetDefault<UVTGMediaLoadingPageSettings>()->OverlayWidgetClass.LoadSynchronous();
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
            .Duration(GetDefault<UVTGMediaLoadingPageSettings>()->FadeInDuration)[PreviewWidget->TakeWidget()];
    ApplyLevelContent(PreviewWidget, LevelName.IsNone() ? World->GetPackage()->GetName() : LevelName.ToString());
    PreviewViewport = Viewport;
    Viewport->AddViewportWidgetContent(PreviewFade.ToSharedRef(), 10000);
    World->GetTimerManager().SetTimer(PreviewTimer, this, &UVTGMediaLoadingPageSystem::HideLoadingPagePreview,
                                      FMath::Max(Duration, 0.1f), false);
}

void UVTGMediaLoadingPageSystem::HideLoadingPagePreview()
{
    if (UWorld *World = GetWorld())
        World->GetTimerManager().ClearTimer(PreviewTimer);
    if (!PreviewFade || PreviewFadeTicker.IsValid())
        return;
    PreviewFade->BeginFadeOut(GetDefault<UVTGMediaLoadingPageSettings>()->FadeOutDuration);
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
    // MoviePlayer's Slate thread has stopped. Hand the same widget to the game viewport
    // before the next frame; keep its UMG owner alive until the fade has finished.
    UGameViewportClient *Viewport = GetGameInstance()->GetGameViewportClient();
    const float Duration = GetDefault<UVTGMediaLoadingPageSettings>()->FadeOutDuration;
    if (LoadingFade && Viewport && Duration > 0.0f)
    {
        FadeViewport = Viewport;
        LoadingFade->BeginFadeOut(Duration);
        if (!bPIELoading)
            Viewport->AddViewportWidgetContent(LoadingFade.ToSharedRef(), 10000);
        FadeTicker = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &UVTGMediaLoadingPageSystem::TickFadeOut));
        UE_LOG(LogTemp, Display, TEXT("VTG loading complete: fading out over new level (%.2fs)."), Duration);
    }
    else
        ClearLoadingTransition();
}

bool UVTGMediaLoadingPageSystem::TickFadeOut(float DeltaTime)
{
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

bool UVTGMediaLoadingPageSystem::StartPIELoadingPage()
{
    const auto *Settings = GetDefault<UVTGMediaLoadingPageSettings>();
    UGameViewportClient *Viewport = GetGameInstance()->GetGameViewportClient();
    if (!bRuntimeEnabled || !Settings->bEnabled || !Viewport || !FApp::CanEverRender())
        return false;
    UClass *WidgetClass = Settings->OverlayWidgetClass.LoadSynchronous();
    if (!WidgetClass)
        return false;
    ClearPreview();
    ClearLoadingTransition();
    ActiveLoadingWidget = CreateWidget<UUserWidget>(GetGameInstance(), WidgetClass);
    if (!ActiveLoadingWidget)
        return false;
    LoadingFade = SNew(SVTGLoadingFade).Duration(Settings->FadeInDuration)[ActiveLoadingWidget->TakeWidget()];
    ApplyLevelContent(ActiveLoadingWidget, DestinationLevel);
    FadeViewport = Viewport;
    Viewport->AddViewportWidgetContent(LoadingFade.ToSharedRef(), 10000);
    bPIELoading = true;
    bPIEMapReady = false;
    PIELoadingStart = FPlatformTime::Seconds();
    PIETravelTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UVTGMediaLoadingPageSystem::TickPIELoading));
    UE_LOG(LogTemp, Display, TEXT("VTG PIE loading screen shown before travel."));
    return true;
}

void UVTGMediaLoadingPageSystem::HandlePostLoadMap(UWorld *World)
{
    if (bPIELoading && World && World->GetGameInstance() == GetGameInstance())
    {
        bPIEMapReady = true;
        UE_LOG(LogTemp, Display, TEXT("VTG PIE destination ready: %s"), *World->GetPackage()->GetName());
    }
}

bool UVTGMediaLoadingPageSystem::TickPIELoading(float DeltaTime)
{
    if (!bPIELoading || !LoadingFade)
    {
        PIETravelTicker.Reset();
        return false;
    }
    if (!PendingLevel.IsNone() && LoadingFade->GetRenderOpacity() >= 0.999f)
    {
        const FName Level = PendingLevel;
        PendingLevel = NAME_None;
        UE_LOG(LogTemp, Display, TEXT("VTG PIE fade-in complete; opening %s"), *Level.ToString());
        UGameplayStatics::OpenLevel(this, Level, bPendingAbsolute, PendingOptions);
    }
    if (bPIEMapReady && FPlatformTime::Seconds() - PIELoadingStart >=
                            FMath::Max(GetDefault<UVTGMediaLoadingPageSettings>()->MinimumDisplayTime,
                                       GetDefault<UVTGMediaLoadingPageSettings>()->FadeInDuration))
    {
        PIETravelTicker.Reset();
        HandlePlaybackFinished(); // Reuse the existing viewport overlay rather than adding it twice.
        bPIELoading = false;
        return false;
    }
    return true;
}

void UVTGMediaLoadingPageSystem::CancelPIELoading()
{
    FTSTicker::GetCoreTicker().RemoveTicker(PIETravelTicker);
    PIETravelTicker.Reset();
    bPIELoading = false;
    bPIEMapReady = false;
    PendingLevel = NAME_None;
}

void UVTGMediaLoadingPageSystem::ApplyLevelContent(UUserWidget *Widget, const FString &LevelName)
{
    if (const auto *Info = GetDefault<UVTGMediaLoadingPageSettings>()->LevelLoadingInfo.LoadSynchronous())
        Info->ApplyToWidget(Widget, LevelName);
}
