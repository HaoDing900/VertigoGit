#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/VTGSaveTypes.h"
#include "VTGSaveCoordinator.generated.h"

class UNarrativeComponent;
class AVTGLevelManagerBase;
class UVTGSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGOnSlotSaved, int32, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGOnSlotLoaded, int32, Slot);

/**
 * Bind these in Blueprint to drive the systems C++ can't reach - ISX inventory's SaveLoad module,
 * for example. When the coordinator writes slot N it fires OnSaveSubsystems(N); when it reads slot
 * N (after the map has loaded) it fires OnLoadSubsystems(N). Your Blueprint binds them and calls
 * the ISX save/load to the same slot. This keeps the coordinator decoupled from any one plugin.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGOnSaveSubsystems, int32, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGOnLoadSubsystems, int32, Slot);

/**
 * The conductor of the whole save system. It owns slot management, ordering, and the load-time
 * "open the right map, THEN restore state" sequence. It deliberately does NOT serialise quests or
 * inventory itself:
 *   - Narrative keeps its own save file, driven here directly in C++ (Save/Load on its component).
 *   - ISX keeps its own save file, driven via the OnSaveSubsystems / OnLoadSubsystems delegates.
 *   - Vertigo-owned state (player status + IVTGSaveable level actors) is serialised by this class.
 * A single logical "slot" = all of those files written/read together for the same index.
 *
 * It lives on the GameInstance (a subsystem) so it survives Open Level - essential, because Load
 * has to hold the pending data across the map change and apply it once the new world exists. Being
 * a subsystem also means there's nothing to reparent: it auto-instantiates and is reachable from
 * any Blueprint via "Get Game Instance Subsystem -> VTG Save Coordinator".
 */
UCLASS()
class VERTIGO_API UVTGSaveCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

 void RestoreLevelManager(AVTGLevelManagerBase* Manager) const;

	// ---- Public API: call these from your menu / BPLM ----

	/** Write everything to slot N. UserLabel is shown in the load menu (e.g. a player-typed name). */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveToSlot(int32 Slot, const FString& UserLabel);

 /** Reuse this terminal's manual slot, or take an empty manual slot; never overwrite another save. */
 UFUNCTION(BlueprintCallable, Category="Save")
 bool SaveFromTerminal(FText& Result);
 UFUNCTION(BlueprintPure, Category="Save")
 int32 GetLatestValidSlot() const;
 UFUNCTION(BlueprintCallable, Category="Save")
 bool ContinueLatestSave();
 UFUNCTION(BlueprintCallable, Category="Save")
 bool SaveTerminalSlot(int32 Slot, FText& Result);

	/** Read slot N: opens its map, then restores player + actors + subsystems once it has loaded. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadFromSlot(int32 Slot);

	/** Convenience: save to the reserved autosave slot. Call from BPLM at your checkpoints. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool AutoSave();

	/**
	 * Forget the run in progress: checkpoint flags, per-map progress, the autosave beat stamp and the
	 * stage. Called automatically when the main menu map loads with no load pending, so a New Game
	 * button needs no wiring; also exposed in case you want to call it explicitly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void BeginNewGame();

	/** Convenience: reload the last autosave checkpoint. Use this for "Press R to restart". */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadAutoSave() { return LoadFromSlot(AutoSaveSlot); }

	/** True if an autosave checkpoint exists (so you can gate the restart prompt). */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasAutoSave() const { return DoesSlotExist(AutoSaveSlot); }

 /**
  * Story autosave: call this on stage begin, i.e. once per level manager BeginPlay. Writes its own
  * reserved slot, so it never competes with the combat checkpoint (slot 0) or the player's 9 manual
  * slots.
  *
  * Saves when the player has arrived in a map it has not already autosaved in, or when the stage has
  * changed - see StoryAutoSaveBeat. Refuses, deliberately, when neither has changed, when a load is
  * still in flight, or while retrying a checkpoint.
  *
  * It does NOT write immediately. Stage begin runs during BeginPlay, and most maps open straight into
  * an auto-playing entry sequence, so writing a tick later would snapshot the player mid-cutscene: a
  * staging transform, story gates already marked started, and a sequence that never finishes on load.
  * Instead it polls CanSaveNow and writes at the first moment the world is actually quiet, giving up
  * after StoryAutoSaveTimeout rather than waiting forever.
  */
 UFUNCTION(BlueprintCallable, Category="Save")
 bool StoryAutoSave(FName Stage);

 /**
  * The form the level manager uses: the world is the one whose BeginPlay is running, which is the
  * authoritative answer at that moment and does not depend on the GameInstance world context being
  * wired up yet.
  */
 bool StoryAutoSave(UWorld* World, FName Stage);

 /**
  * True when this run must not write story autosaves: any run launched out of the editor - Play,
  * Simulate or Standalone - while VTG.AutoSaveInDevRuns is 0, which is the default.
  *
  * This exists because tuning level sequences means sitting in Simulate for long stretches, and every
  * BPLM BeginPlay schedules a story autosave. Those writes land in the same Saved/SaveGames files the
  * game uses, so an afternoon of sequence work churns slots 10/11 and leaves the main menu's Continue
  * pointing at a test.
  *
  * Keyed on WITH_EDITOR, not on the world type. IsPlayInEditor() would read better but only covers
  * Play and Simulate (both EWorldType::PIE) and misses Standalone, whose world type is Game - and
  * Standalone is exactly what has to be used to test anything the PIE viewport cannot show, such as
  * the tunnel loading movie. WITH_EDITOR is true for every binary the editor launches, Standalone
  * included, and false in a packaged game, so one condition covers all three and the shipped build
  * has no branch here at all.
  *
  * The gate is deliberately narrow: it suppresses the AUTOMATIC story autosave only. A manual terminal
  * save and the pre-fight combat checkpoint (slot 0, which is what makes retry-after-death work)
  * always write, in the editor as in a build, because testing those is the point.
  *
  * It holds no state. Nothing is deferred, queued or remembered while it is on - the save simply does
  * not happen - so flipping the console variable mid-session is not a transition and needs no reset.
  */
 bool IsStoryAutoSaveSuppressed(const UWorld* World) const;

 /**
  * Is it safe to write a save of this world right now? The one place that answers it, so the terminal
  * and the story autosave cannot disagree. OutReason is player-facing and empty when the answer is
  * yes. Refused during combat, during a playing level sequence, while dialogue or a Note owns the UI
  * input, while a load is in flight or a checkpoint retry is running, and with no world or pawn.
  */
 UFUNCTION(BlueprintCallable, Category="Save")
 bool CanSaveNow(UWorld* World, FText& OutReason) const;

 /** Reads the player pawn's own "InCombat" flag, the same one the combat presentation code uses. */
 UFUNCTION(BlueprintPure, Category="Save")
 bool IsPlayerInCombat(UWorld* World) const;

 UFUNCTION(BlueprintPure, Category="Save")
 int32 GetStoryAutoSaveSlot() const { return StoryAutoSaveSlot; }

 /**
  * The story autosave alternates between StoryAutoSaveSlotCount slots instead of overwriting one.
  * A single rolling slot means one bad write - or one save taken in a state nobody anticipated -
  * destroys the last good one; with two, the previous beat is always still there to fall back to.
  */
 UFUNCTION(BlueprintPure, Category="Save")
 bool IsStoryAutoSaveSlot(int32 Slot) const { return Slot>=StoryAutoSaveSlot && Slot<StoryAutoSaveSlot+StoryAutoSaveSlotCount; }

 /** True for a slot the player may write from the terminal: not slot 0, not a story autosave slot. */
 UFUNCTION(BlueprintPure, Category="Save")
 bool IsManualSlot(int32 Slot) const { return Slot>AutoSaveSlot && Slot<MaxSlots && !IsStoryAutoSaveSlot(Slot); }

	// ---- Checkpoints ----------------------------------------------------------------------------
	// A checkpoint is the autosave slot plus three persistent flags (id, level, order), so it rides on
	// everything above: loading reopens the map, restores Stage/flags before BeginPlay, then player,
	// actors, quests and inventory one tick later. AVTGCheckpoint calls SaveCheckpoint for you.

	/**
	 * Autosave as checkpoint CheckpointId; on retry the player respawns at RespawnTransform (not where
	 * they happened to be standing). Ignored if this level already has a checkpoint with a higher Order,
	 * so walking back over an earlier checkpoint never moves the respawn backwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Checkpoint")
	bool SaveCheckpoint(FName CheckpointId, const FTransform& RespawnTransform, int32 Order = 0);

	/**
	 * Death screen "Retry": fade to black, reload this level's last checkpoint without the loading
	 * video, fade back in once everything is restored. No checkpoint in this level yet = restart the
	 * level from its beginning (same stage).
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Checkpoint")
	bool RetryFromCheckpoint();

	/**
	 * The checkpoint this level was just loaded from, or None for a normal level start. Valid from
	 * BeginPlay on (Stage and flags are restored before the map opens), for the whole stay in the
	 * level. Branch on it in BPLM's OnStageBegin to skip intros the player has already seen.
	 */
	UFUNCTION(BlueprintPure, Category = "Save|Checkpoint")
	FName GetResumeCheckpoint() const { return ResumedCheckpoint; }

	/** Order of the checkpoint returned by GetResumeCheckpoint (meaningless when that is None). */
	UFUNCTION(BlueprintPure, Category = "Save|Checkpoint")
	int32 GetResumeCheckpointOrder() const { return GetPersistentInt(CheckpointOrderKey, 0); }

	/** New game: delete the checkpoint save and forget the checkpoint flags. */
	UFUNCTION(BlueprintCallable, Category = "Save|Checkpoint")
	void ClearCheckpoint();

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool DeleteSlot(int32 Slot);

	UFUNCTION(BlueprintPure, Category = "Save")
	bool DoesSlotExist(int32 Slot) const;

	/** For the load menu: metadata for every slot 0..MaxSlots-1. Empty slots come back with bIsValid=false. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void GetAllSlotMetas(TArray<FVTGSlotMeta>& OutMetas) const;

	UFUNCTION(BlueprintPure, Category = "Save")
	int32 GetAutoSaveSlot() const { return AutoSaveSlot; }

	// ---- Persistent checkpoint flags ------------------------------------------------------------
	// These survive a load and, crucially, are restored BEFORE the saved map opens - so an actor can
	// read them in its BeginPlay and decide what to do (e.g. "skip the intro, go straight to combat").
	// They do NOT touch the Stage, so stage-gated actors are not destroyed. Set a flag right before
	// AutoSave; read it on level start.

	/** Set a checkpoint int (use 0/1 for a bool, or an enum's index). */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetPersistentInt(FName Key, int32 Value) { PersistentInts.Add(Key, Value); }

	UFUNCTION(BlueprintPure, Category = "Save")
	int32 GetPersistentInt(FName Key, int32 DefaultValue = 0) const
	{
		const int32* Found = PersistentInts.Find(Key);
		return Found ? *Found : DefaultValue;
	}

	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetPersistentName(FName Key, FName Value) { PersistentNames.Add(Key, Value); }

	UFUNCTION(BlueprintPure, Category = "Save")
	FName GetPersistentName(FName Key) const
	{
		const FName* Found = PersistentNames.Find(Key);
		return Found ? *Found : NAME_None;
	}

	/** Wipe all checkpoint flags (e.g. when starting a brand-new game). */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void ClearPersistentFlags() { PersistentInts.Reset(); PersistentNames.Reset(); ProgressByMap.Reset(); }

	// ---- Hooks for Blueprint-only subsystems (bind ISX here) ----

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FVTGOnSaveSubsystems OnSaveSubsystems;

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FVTGOnLoadSubsystems OnLoadSubsystems;

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FVTGOnSlotSaved OnSlotSaved;

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FVTGOnSlotLoaded OnSlotLoaded;

	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

 /** Scene resume hooks must wait until actor, inventory and narrative restoration finishes. */
 bool HasPendingWorldRestore() const { return PendingLoad != nullptr; }

protected:

	/** Reserved slot used by AutoSave. Manual saves should use other indices (1..MaxSlots-1). */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 AutoSaveSlot = 0;

	/** How many slots the load menu enumerates. */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 MaxSlots = 12;

	/**
	 * Reserved for the story autosave. Outside the manual 1..9 range on purpose: a story beat must
	 * never overwrite a combat checkpoint, and neither may take one of the player's own slots.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 StoryAutoSaveSlot = 10;

	/** How many slots the story autosave rotates through, starting at StoryAutoSaveSlot. */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 StoryAutoSaveSlotCount = 2;

	/** Give up waiting for a quiet moment after this long; a very long cutscene should not block it forever. */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	float StoryAutoSaveTimeout = 180.f;

	/** How often the pending story autosave re-checks CanSaveNow while it waits for a quiet moment. */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	float StoryAutoSavePollInterval = 0.5f;

	/**
	 * Loading this map with nothing pending means the player left their run, so the coordinator resets.
	 * Short name, no path, as GetCurrentLevelName reports it. Empty disables the automatic reset.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	FString MainMenuLevelName = TEXT("M_MainMenu");

	/**
	 * Actor classes whose destruction is NOT remembered, i.e. ones the map is allowed to bring back on
	 * load. The player pawn and level managers are excluded in code; add anything else here rather than
	 * in C++. Everything else that came with the map and was destroyed stays destroyed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	TArray<TSubclassOf<AActor>> DestructionUntrackedClasses;

	/**
	 * GameInstance Blueprint variables left out of the snapshot. "SaveData" is Inventory System X's
	 * own save blob living on the GameInstance: ISX saves and loads it itself, so copying it here
	 * would only let a stale duplicate fight the real one. Object references are never snapshotted.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	TSet<FName> ExcludedGameInstanceVariables { FName(TEXT("SaveData")) };

private:
 friend struct FVTGTerminalSaveRegression;
 friend struct FVTGLevelProgressRegression;
 friend struct FVTGGameInstanceSnapshotRegression;
 friend struct FVTGStoryAutoSaveRegression;
 friend struct FVTGL2FightRegression;
 friend struct FVTGEncounterCheckpointRegression;
 friend struct FVTGStreetSewerCheckpointRegression;

	static FString SaveSlotName(int32 Slot);
	static FString ManifestSlotName(int32 Slot);
	static FString NarrativeSaveName(int32 Slot);

	static FName ResolveSaveId(AActor* Actor);
	void GatherWorldState(UWorld* World, UVTGSaveGame* SaveObj);
	void ApplyWorldState(UWorld* World, UVTGSaveGame* SaveObj);

	UNarrativeComponent* FindNarrativeComponent(UWorld* World) const;

	/** Bound to PostLoadMapWithWorld: applies PendingLoad once the loaded map's actors exist. */
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void CancelPendingRestore();
	FTSTicker::FDelegateHandle RestoreTicker;
	FTSTicker::FDelegateHandle RetryRevealTicker;
	FTSTicker::FDelegateHandle StoryAutoSaveTicker;

	/** PersistentNames key holding the stage the last story autosave was written for. */
	static const FName StoryAutoSaveStageKey;

	/** Map name + stage, the unit the story autosave de-duplicates on. None when there is no world. */
	FName StoryAutoSaveBeat(UWorld* World, FName Stage) const;

	/** The rotating slot to write next: the one with no save, else the older of the two. */
	int32 PickStoryAutoSaveSlot() const;

	/** Deadline for the pending story autosave, in FPlatformTime::Seconds. */
	double StoryAutoSaveGiveUpTime = 0.0;

	/** True once the spinner has been put up for the pending autosave, so the write waits a frame. */
	bool bStoryAutoSaveAnnounced = false;

	/** The bottom-right "saving" spinner. Created on first use; it removes itself when done. */
	UPROPERTY(Transient)
	TObjectPtr<class UVTGSaveIndicator> SaveIndicator;

	void ShowSaveIndicator(UWorld* World);
	void FinishSaveIndicator();

	/** Property on the player pawn that says a fight is on. */
	static const FName CombatFlagProperty;

	/** Cache for that lookup; IsPlayerInCombat is polled, so it must not walk the class every call. */
	mutable TWeakObjectPtr<const UClass> CombatFlagClass;
	mutable const FBoolProperty* CombatFlagCached = nullptr;

	/** Bound to PreLoadMap: a map opened by anything other than a load is not a checkpoint resume. */
	void HandlePreLoadMap(const FString& MapName);

	/** Second half of RetryFromCheckpoint, after the fade-out. */
	void DoRetry();

	/** Every new game world: start a fresh destroyed-actor list and listen for destroys in it. */
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	/** Remembers level-placed saveable actors destroyed in play (a pickup that was taken). */
	void HandleActorDestroyed(AActor* Actor);

	/** False for actors a load must be allowed to bring back (the player, level managers, opt-outs). */
	bool ShouldTrackDestruction(const AActor* Actor) const;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle WorldActorsInitializedHandle;

	/** Save ids of this level's placed saveable actors that no longer exist; written into the save. */
	TSet<FName> DestroyedActorIds;

	/** Used once by GatherWorldState instead of the pawn's transform (checkpoint respawn point). */
	TOptional<FTransform> RespawnOverride;

	/** Set while the current level came from a load; see GetResumeCheckpoint. */
	FName ResumedCheckpoint = NAME_None;

	/** Retry in flight: hide the map change behind black and restore the loading page afterwards. */
	bool bRetrying = false;

	static const FName CheckpointIdKey;
	static const FName CheckpointLevelKey;
	static const FName CheckpointOrderKey;

	/** Save object waiting to be applied after the level finishes opening (the Load path). */
	UPROPERTY()
	TObjectPtr<UVTGSaveGame> PendingLoad = nullptr;

	int32 PendingLoadSlot = INDEX_NONE;

	FDelegateHandle PostLoadMapHandle;

	/** Live checkpoint flags. Persisted into the save and restored before the map opens (see Load). */
	UPROPERTY()
	TMap<FName, int32> PersistentInts;

	UPROPERTY()
	TMap<FName, FName> PersistentNames;

	/**
	 * Per-map progress for the slot currently in play: seeded from the save on load, and updated for
	 * the current map on every save so the file keeps the maps the player already left behind.
	 * Restore still only ever reads the current map's entry - see FVTGMapProgress.
	 */
	UPROPERTY()
	TMap<FString, FVTGMapProgress> ProgressByMap;

	/** Folds the freshly gathered current-map state into ProgressByMap and stamps it on the save. */
	void AccumulateMapProgress(UVTGSaveGame* SaveObj);

	/** Text-exports the GameInstance's Blueprint value variables into the save. */
	void CaptureGameInstanceValues(UVTGSaveGame* SaveObj);

	/** Applies that snapshot back onto the live GameInstance. Must run BEFORE the map opens. */
	void RestoreGameInstanceValues(const UVTGSaveGame* SaveObj);
};
