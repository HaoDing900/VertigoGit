#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/VTGSaveTypes.h"
#include "VTGSaveCoordinator.generated.h"

class UNarrativeComponent;
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

	// ---- Public API: call these from your menu / BPLM ----

	/** Write everything to slot N. UserLabel is shown in the load menu (e.g. a player-typed name). */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveToSlot(int32 Slot, const FString& UserLabel);

	/** Read slot N: opens its map, then restores player + actors + subsystems once it has loaded. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadFromSlot(int32 Slot);

	/** Convenience: save to the reserved autosave slot. Call from BPLM at your checkpoints. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool AutoSave();

	/** Convenience: reload the last autosave checkpoint. Use this for "Press R to restart". */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadAutoSave() { return LoadFromSlot(AutoSaveSlot); }

	/** True if an autosave checkpoint exists (so you can gate the restart prompt). */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasAutoSave() const { return DoesSlotExist(AutoSaveSlot); }

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
	void ClearPersistentFlags() { PersistentInts.Reset(); PersistentNames.Reset(); }

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

protected:

	/** Reserved slot used by AutoSave. Manual saves should use other indices (1..MaxSlots-1). */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 AutoSaveSlot = 0;

	/** How many slots the load menu enumerates. */
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 MaxSlots = 10;

private:

	static FString SaveSlotName(int32 Slot);
	static FString ManifestSlotName(int32 Slot);
	static FString NarrativeSaveName(int32 Slot);

	static FName ResolveSaveId(AActor* Actor);
	void GatherWorldState(UWorld* World, UVTGSaveGame* SaveObj);
	void ApplyWorldState(UWorld* World, UVTGSaveGame* SaveObj);

	UNarrativeComponent* FindNarrativeComponent(UWorld* World) const;

	/** Bound to PostLoadMapWithWorld: applies PendingLoad once the loaded map's actors exist. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** Bound to PreLoadMap: a map opened by anything other than a load is not a checkpoint resume. */
	void HandlePreLoadMap(const FString& MapName);

	/** Second half of RetryFromCheckpoint, after the fade-out. */
	void DoRetry();

	/** Every new game world: start a fresh destroyed-actor list and listen for destroys in it. */
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	/** Remembers level-placed saveable actors destroyed in play (a pickup that was taken). */
	void HandleActorDestroyed(AActor* Actor);

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
};
