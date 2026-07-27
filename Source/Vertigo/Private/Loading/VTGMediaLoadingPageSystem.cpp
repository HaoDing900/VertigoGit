#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Loading/VTGMediaLoadingPageSettings.h"
#include "MoviePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectGlobals.h"

void UVTGMediaLoadingPageSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Fires at the start of every map change (OpenLevel / travel) - the correct moment to arm the
	// MoviePlayer, before the engine flushes and blocks on the new level.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UVTGMediaLoadingPageSystem::HandlePreLoadMap);
}

void UVTGMediaLoadingPageSystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	Super::Deinitialize();
}

void UVTGMediaLoadingPageSystem::HandlePreLoadMap(const FString& MapName)
{
	SetupLoadingPage();
}

void UVTGMediaLoadingPageSystem::ArmLoadingPage()
{
	SetupLoadingPage();
}

void UVTGMediaLoadingPageSystem::OpenLevelWithLoadingPage(FName LevelName, bool bAbsolute, const FString& Options)
{
	// Arm first so the page is registered before the travel begins, then go.
	SetupLoadingPage();
	UGameplayStatics::OpenLevel(this, LevelName, bAbsolute, Options);
}

void UVTGMediaLoadingPageSystem::SetupLoadingPage()
{
	if (!bRuntimeEnabled || !FApp::CanEverRender())
	{
		return;
	}

	const UVTGMediaLoadingPageSettings* Settings = GetDefault<UVTGMediaLoadingPageSettings>();
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

	FLoadingScreenAttributes Attributes;
	Attributes.MinimumLoadingScreenDisplayTime = Settings->MinimumDisplayTime;
	// Dismiss once the new map is ready (but never before MinimumLoadingScreenDisplayTime).
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = Settings->bSkippable;
	// MT_LoadingLoop loops the last clip until loading finishes - hides variable load time.
	Attributes.PlaybackType = Settings->bLoopUntilLoaded ? MT_LoadingLoop : MT_Normal;
	Attributes.MoviePaths = Settings->MovieNames;

	// Optional UMG overlay composited over the movie (vignette / title / grain).
	if (bHasOverlay)
	{
		if (UClass* WidgetClass = Settings->OverlayWidgetClass.LoadSynchronous())
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UUserWidget* Widget = CreateWidget<UUserWidget>(GI, WidgetClass))
				{
					Attributes.WidgetLoadingScreen = Widget->TakeWidget();
				}
			}
		}
	}

	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}
