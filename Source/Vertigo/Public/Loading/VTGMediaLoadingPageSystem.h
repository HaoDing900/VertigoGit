#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VTGMediaLoadingPageSystem.generated.h"

/**
 * Plays a media loading page (a pre-rendered clip) during level transitions. Flagship use is the tunnel
 * ride between levels, but it's generic - any looping movie works.
 *
 * Why a GameInstance subsystem and not an ActorComponent (like VTGParallaxScrollComponent): a loading
 * transition spans the moment BETWEEN two levels - the old world is tearing down and the new one isn't
 * up yet. An actor/component would die with its level. The subsystem lives for the whole game, so it can
 * arm the engine MoviePlayer (which renders on its own thread) right as the map swap begins.
 *
 * Config lives in Project Settings > Game > "VTG Media Loading Page" (UVTGMediaLoadingPageSettings).
 *
 * Usage:
 *   - Easiest: call OpenLevelWithLoadingPage(...) instead of OpenLevel(...) - it arms the page then travels.
 *   - Automatic: any OpenLevel / travel also triggers it, because we hook PreLoadMap. So even plain
 *     UGameplayStatics::OpenLevel elsewhere gets the page (unless disabled).
 *
 * PIE uses a viewport overlay and deferred travel; standalone uses MoviePlayer. Test a
 * packaged build separately when changing movies or cook configuration.
 */
class UUserWidget;

UCLASS()
class VERTIGO_API UVTGMediaLoadingPageSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

  public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    /** Arm the media loading page, then travel to LevelName. Use this in place of OpenLevel. */
    UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
    void OpenLevelWithLoadingPage(FName LevelName, bool bAbsolute = true, const FString &Options = TEXT(""));

    /** Menu travel entry: includes a pre-travel fade in PIE as well as standalone. */
    UFUNCTION(BlueprintCallable, Category = "Media Loading Page",
              meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "2",
                      DisplayName = "Open Level With Loading Screen (Object Reference)"))
    static void OpenLevelWithLoadingScreen(const UObject *WorldContextObject, TSoftObjectPtr<UWorld> Level,
                                           bool bAbsolute = true, FString Options = TEXT(""));

    UFUNCTION(BlueprintPure, Category = "Media Loading Page")
    bool IsLoadingPageVisible() const
    {
        return LoadingFade.IsValid();
    }

    /** Manually arm the page for the next map load (if you kick off the travel yourself elsewhere). */
    UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
    void ArmLoadingPage();

    /** Runtime on/off without touching Project Settings (e.g. skip the page for a quick reload). */
    UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
    void SetLoadingPageEnabled(bool bInEnabled)
    {
        bRuntimeEnabled = bInEnabled;
    }

    /** Preview the configured UMG screen in PIE; automatically hides after Duration seconds. */
    UFUNCTION(BlueprintCallable, Category = "Media Loading Page", meta = (ClampMin = "0.1"))
    void PreviewLoadingPage(float Duration = 5.0f, FName LevelName = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
    void HideLoadingPagePreview();

  private:
    // Keep the UMG owner alive across world teardown and loading-time garbage collection.
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveLoadingWidget;
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> PreviewWidget;
    FTimerHandle PreviewTimer;
    TSharedPtr<class SVTGLoadingFade> LoadingFade;
    TSharedPtr<class SVTGLoadingFade> PreviewFade;
    TWeakObjectPtr<class UGameViewportClient> FadeViewport;
    TWeakObjectPtr<class UGameViewportClient> PreviewViewport;
    FTSTicker::FDelegateHandle FadeTicker;
    FTSTicker::FDelegateHandle PreviewFadeTicker;
    void ClearLoadingTransition();
    void ClearPreview();
    bool TickFadeOut(float DeltaTime);
    bool TickPreviewFadeOut(float DeltaTime);

    /** Hooked to FCoreUObjectDelegates::PreLoadMap so every travel arms the page automatically. */
    void HandlePreLoadMap(const FString &MapName);

    /** Build FLoadingScreenAttributes from settings and hand them to the engine MoviePlayer. */
    void SetupLoadingPage();
    void ApplyLevelContent(UUserWidget *Widget, const FString &LevelName);
    FString DestinationLevel;

    void HandlePostLoadMap(UWorld *World);
    bool StartPIELoadingPage();
    bool TickPIELoading(float DeltaTime);
    void CancelPIELoading();
    FTSTicker::FDelegateHandle PIETravelTicker;
    FDelegateHandle PostLoadMapHandle;
    FDelegateHandle TravelFailureHandle;
    bool bPIELoading = false;
    bool bPIEMapReady = false;
    FName PendingLevel;
    FString PendingOptions;
    bool bPendingAbsolute = true;
    double PIELoadingStart = 0.0;
    FDelegateHandle PreLoadMapHandle;
    FDelegateHandle PlaybackFinishedHandle;
    void HandlePlaybackFinished();

    /** Gate separate from the settings' bEnabled, for temporary runtime suppression. */
    bool bRuntimeEnabled = true;
};
