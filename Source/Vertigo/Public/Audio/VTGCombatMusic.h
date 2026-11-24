#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "VTGCombatMusic.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * One fight's music. The row NAME is the track id a level asks for ("StreetBarFight"), so the
 * wiring never names a sound file - swapping the source or the level is a table edit.
 */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGCombatMusicRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 音源 / The track. A plain SoundWave is fine: the subsystem restarts it, so it need not loop itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Music",
		meta = (AllowedClasses = "/Script/Engine.SoundBase", DisplayName = "音源 / Music"))
	TSoftObjectPtr<USoundBase> Music;

	/** 音量 / Volume multiplier applied when the track fades in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Music",
		meta = (ClampMin = "0", UIMax = "2", DisplayName = "音量 / Volume"))
	float Volume = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Music",
		meta = (ClampMin = "0", DisplayName = "淡入秒数 / Fade In"))
	float FadeIn = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Music",
		meta = (ClampMin = "0", DisplayName = "淡出秒数 / Fade Out"))
	float FadeOut = 3.f;
};

/** Where the one combat-music table lives. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "VTG Combat Music"))
class VERTIGO_API UVTGCombatMusicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Vertigo"); }

	/** 战斗音乐表。行名就是 track id，例如 StreetBarFight */
	UPROPERTY(config, EditAnywhere, Category = "Combat Music",
		meta = (AllowedClasses = "/Script/Engine.DataTable", DisplayName = "战斗音乐表 / Combat Music Tracks"))
	TSoftObjectPtr<UDataTable> Tracks;
};

/**
 * The one 2D music channel a fight owns.
 *
 * A fight asks for a track id when it starts and gives it back when it ends; the subsystem keeps the
 * audio component alive and restarts it on OnAudioFinished, so the track loops for as long as the
 * fight lasts whatever the source asset's own looping flag says. Asking for the track that is
 * already playing does nothing, so an encounter may call it on every re-entry.
 */
UCLASS()
class VERTIGO_API UVTGCombatMusicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Starts (or keeps) Track. An unknown id stops whatever is playing and logs, rather than leaving the old fight's music up. */
	void Start(FName Track);

	/** FadeOutOverride < 0 uses the row's own fade out. */
	void Stop(float FadeOutOverride = -1.f);

	FName CurrentTrack() const { return Playing; }

	virtual void Deinitialize() override;

	static bool FindRow(const UObject* WorldContext, FName Track, FVTGCombatMusicRow& OutRow);

private:
	UFUNCTION()
	void HandleFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;

	FName Playing;
	float ActiveFadeOut = 3.f;
};

UCLASS()
class VERTIGO_API UVTGCombatMusicLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 战斗开始时调用。Loops until Stop Combat Music. */
	UFUNCTION(BlueprintCallable, Category = "VTG|Audio",
		meta = (WorldContext = "WorldContextObject", DisplayName = "VTG Start Combat Music"))
	static void StartCombatMusic(const UObject* WorldContextObject, FName Track);

	/** 战斗结束时调用。Safe to call when nothing is playing. */
	UFUNCTION(BlueprintCallable, Category = "VTG|Audio",
		meta = (WorldContext = "WorldContextObject", DisplayName = "VTG Stop Combat Music", AdvancedDisplay = "FadeOutOverride"))
	static void StopCombatMusic(const UObject* WorldContextObject, float FadeOutOverride = -1.f);

	/** The track id currently playing, or None. */
	UFUNCTION(BlueprintPure, Category = "VTG|Audio", meta = (WorldContext = "WorldContextObject"))
	static FName GetCombatMusicTrack(const UObject* WorldContextObject);
};
