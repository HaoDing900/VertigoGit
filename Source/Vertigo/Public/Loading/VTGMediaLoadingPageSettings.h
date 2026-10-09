#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VTGMediaLoadingPageSettings.generated.h"

class UUserWidget;
class UVTGLevelLoadingInfo;

/**
 * Project-wide config for the media loading page - a pre-rendered clip shown while the next level
 * streams in. The flagship use is the Wong-Kar-wai "Fallen Angels" tunnel ride between levels, but this
 * is generic: any looping movie works. Edit under Project Settings > Game > "VTG Media Loading Page".
 *
 * We use the engine MoviePlayer so the clip renders on its own thread and keeps playing during the
 * blocking map load. Put the movie in Content/Movies (see Docs/TunnelLoading.md).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "VTG Media Loading Page"))
class VERTIGO_API UVTGMediaLoadingPageSettings : public UDeveloperSettings
{
    GENERATED_BODY()

  public:
    virtual FName GetCategoryName() const override
    {
        return TEXT("Game");
    }

    /** Master switch. Off = normal instant level loads with no loading page. */
    UPROPERTY(config, EditAnywhere, Category = "Media Loading Page")
    bool bEnabled = true;

    /**
     * Movie file names in Content/Movies (name only, no path or extension). Played in order;
     * with bLoopUntilLoaded the LAST one loops until the level is ready. Put your tunnel-ride .mp4 here.
     */
    UPROPERTY(config, EditAnywhere, Category = "Media Loading Page")
    TArray<FString> MovieNames;

    /** Minimum seconds the page stays up even if the level loads instantly - your 2-3s romance. */
    UPROPERTY(config, EditAnywhere, Category = "Media Loading Page", meta = (ClampMin = "0.0", UIMax = "10.0"))
    float MinimumDisplayTime = 2.5f;

    /** Fade the loading layout in on MoviePlayer's Slate thread. */
    UPROPERTY(config, EditAnywhere, Category = "Transitions", meta = (ClampMin = "0.0", UIMax = "3.0"))
    float FadeInDuration = 0.4f;

    /** Fade the loading layout away over the new world after map loading finishes. */
    UPROPERTY(config, EditAnywhere, Category = "Transitions", meta = (ClampMin = "0.0", UIMax = "3.0"))
    float FadeOutDuration = 0.6f;

    /** Loop the last clip until loading finishes (recommended - hides variable load time). */
    UPROPERTY(config, EditAnywhere, Category = "Media Loading Page")
    bool bLoopUntilLoaded = true;

    /** Let the player skip the clip once the level is actually ready. */
    UPROPERTY(config, EditAnywhere, Category = "Media Loading Page")
    bool bSkippable = false;

    /** Per-destination text data. Does not modify the Widget Blueprint's layout or styling. */
    UPROPERTY(config, EditAnywhere, Category = "Level Content",
              meta = (DisplayName = "关卡加载文案 / Level Loading Info"))
    TSoftObjectPtr<UVTGLevelLoadingInfo> LevelLoadingInfo;

    /** Optional UMG overlay drawn ON TOP of the movie (vignette, title card, grain, subtitle). */
    UPROPERTY(config, EditAnywhere, Category = "Media Loading Page")
    TSoftClassPtr<UUserWidget> OverlayWidgetClass;
};
