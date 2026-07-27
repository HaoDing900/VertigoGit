#pragma once

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
 * NOTE: loading pages do NOT show in PIE - test in Standalone ("New Editor Window (Standalone)") or a
 * packaged build.
 */
UCLASS()
class VERTIGO_API UVTGMediaLoadingPageSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Arm the media loading page, then travel to LevelName. Use this in place of OpenLevel. */
	UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
	void OpenLevelWithLoadingPage(FName LevelName, bool bAbsolute = true, const FString& Options = TEXT(""));

	/** Manually arm the page for the next map load (if you kick off the travel yourself elsewhere). */
	UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
	void ArmLoadingPage();

	/** Runtime on/off without touching Project Settings (e.g. skip the page for a quick reload). */
	UFUNCTION(BlueprintCallable, Category = "Media Loading Page")
	void SetLoadingPageEnabled(bool bInEnabled) { bRuntimeEnabled = bInEnabled; }

private:
	/** Hooked to FCoreUObjectDelegates::PreLoadMap so every travel arms the page automatically. */
	void HandlePreLoadMap(const FString& MapName);

	/** Build FLoadingScreenAttributes from settings and hand them to the engine MoviePlayer. */
	void SetupLoadingPage();

	FDelegateHandle PreLoadMapHandle;

	/** Gate separate from the settings' bEnabled, for temporary runtime suppression. */
	bool bRuntimeEnabled = true;
};
