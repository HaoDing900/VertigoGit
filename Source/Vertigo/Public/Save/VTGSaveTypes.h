#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Components/InventoryComponent.h"
#include "VTGSaveTypes.generated.h"

/**
 * Bump this whenever the saved data layout changes in a way old saves can't be read 1:1.
 * The coordinator stamps it into every save and you can branch on it in a migration step later.
 * Saving this from day one is the difference between "migrate old saves" and "every playtest is bricked".
 */
#define VTG_SAVE_VERSION 3 // 2: player inventory, player as an IVTGSaveable record, destroyed actors; 3: ProgressByMap + GameInstance snapshot

/**
 * Lightweight, human-facing description of a save slot. This is all the load menu needs to draw a
 * row, so it is stored in its own tiny manifest file (UVTGSlotManifest) and can be read without
 * loading the full - potentially large - save object.
 */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGSlotMeta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 SlotIndex = 0;

	/** "Autosave", or a player-typed name for a manual save. */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FString DisplayLabel;

	/** The stage the game was in (mirrors UVTGGameInstanceBase::CurrentStage). */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FName Stage = NAME_None;

	/** Map this save lives on - we must open it before restoring actor state. */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FString LevelName;

	/** Wall-clock time the save was written, for sorting / display in the menu. */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FDateTime SaveTimeUtc;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 SaveVersion = VTG_SAVE_VERSION;

	/** True for a slot that exists on disk; false for an empty slot row the menu shows. */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	bool bIsValid = false;
};

/**
 * Per-actor state record. Keyed in the save by the actor's stable Save Id (see IVTGSaveable).
 * Data is the actor's SaveGame-tagged properties serialised to bytes.
 */
USTRUCT()
struct VERTIGO_API FVTGActorRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName SaveId = NAME_None;

	UPROPERTY()
	TArray<uint8> Data;

	UPROPERTY()
	bool bHasTransform = false;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;
};

/** BPLM progress only; runtime object references are rebuilt by the level. */
USTRUCT()
struct VERTIGO_API FVTGLevelProgressRecord
{
 GENERATED_BODY()
 UPROPERTY() FString ClassPath;
 UPROPERTY() TMap<FName, FString> Values;
 UPROPERTY() TMap<FName, FString> Types;
 UPROPERTY() TSet<FName> StartedEvents;
 /** Parameterless BPLM event that rebuilds the checkpoint scene after all subsystems load. */
 UPROPERTY() FName EncounterResumeEvent;
 UPROPERTY() FName EncounterCheckpointId;
 /**
  * The sub-stage this BPLM was in when the save was written - the story checkpoint the hard initiate
  * should rebuild the scene from. None means "the stage's normal setup".
  *
  * A field of its own rather than a Blueprint variable: the progress snapshot only collects variables
  * declared on the BPLM itself, so a base-class one would be dropped silently, and a BPLM_C one would
  * depend on a SaveGame checkbox nobody can see from the graph.
  */
 UPROPERTY() FName SubStage;
};

/**
 * Everything one map owns, keyed per map so leaving and coming back has somewhere to read from.
 *
 * Today the coordinator only ever RESTORES the entry for Meta.LevelName, which is exactly what it
 * did before this struct existed - so behaviour is unchanged. What changed is that the file now
 * keeps the other maps' entries instead of throwing them away on every save. That half is the
 * expensive half to add later (it needs a format version and a migration); reading them back is
 * cheap and can land whenever the design calls for it.
 */
USTRUCT()
struct VERTIGO_API FVTGMapProgress
{
 GENERATED_BODY()
 /** BPLM progress snapshots, keyed by level manager actor name (as LevelProgress is). */
 UPROPERTY() TMap<FName, FVTGLevelProgressRecord> Managers;
 /** "CanInteract?" on story interactions in this map. */
 UPROPERTY() TMap<FName, bool> InteractionEnabled;
 /** Saveable actors in this map that had already been destroyed (taken pickups...). */
 UPROPERTY() TArray<FName> DestroyedActors;
};

/**
 * The full save payload for one slot - everything Vertigo's own code owns. Narrative keeps its OWN
 * file (the coordinator writes it to a matching slot); this object is the glue state:
 * player status + inventory + level-placed actor state + the slot meta.
 */
UCLASS()
class VERTIGO_API UVTGSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FVTGSlotMeta Meta;

	/** Player status (the UVTGPlayerProgressComponent's SaveGame properties) serialised to bytes. */
	UPROPERTY()
	TArray<uint8> PlayerData;

 /** Terminal saves preserve current health; checkpoint retry retains its existing full-health behavior. */
 UPROPERTY()
 bool bHasTerminalHealth = false;
 UPROPERTY()
 double TerminalHealth = 0.0;

	/** The player's Inventory System X inventory (items, ammo, shortcuts, equipped item). */
	UPROPERTY()
	bool bHasPlayerInventory = false;

	UPROPERTY()
	FInventorySaveData PlayerInventory;

	UPROPERTY()
	bool bHasPlayerTransform = false;

	UPROPERTY()
	FTransform PlayerTransform = FTransform::Identity;

	/** Level-placed stateful actors (BPLM children, doors, taken pickups...), keyed by Save Id. */
	UPROPERTY()
	TMap<FName, FVTGActorRecord> ActorRecords;

 UPROPERTY()
 TMap<FName, FVTGLevelProgressRecord> LevelProgress;
 UPROPERTY() TMap<FName, bool> InteractionEnabled;
 UPROPERTY() TMap<FName, FString> PlayerMail;

	/** Level-placed saveable actors that had been destroyed (taken pickups...); removed again on load. */
	UPROPERTY()
	TArray<FName> DestroyedActors;

	/** Checkpoint flags restored BEFORE the map opens, so BeginPlay can branch on them (ints/enums/bools). */
	UPROPERTY()
	TMap<FName, int32> PersistentInts;

	/** Same, for name-valued checkpoint state. */
	UPROPERTY()
	TMap<FName, FName> PersistentNames;

 /**
  * Per-map progress. The entry for Meta.LevelName is the same data as LevelProgress /
  * InteractionEnabled / DestroyedActors above (kept there because that is what the restore path and
  * the editor regression commandlets read); the other entries are maps the player has already been
  * through in this slot, carried forward from save to save. Nothing reads those yet - see
  * FVTGMapProgress.
  */
 UPROPERTY()
 TMap<FString, FVTGMapProgress> ProgressByMap;

 /**
  * Snapshot of the GameInstance's own Blueprint value variables ("Leashed?", day counters, which
  * main quest is current...). The GameInstance survives a level travel, which is exactly why these
  * are the right home for a fact that has to outlive a map change - and why the coordinator restores
  * them BEFORE opening the map, so BeginPlay and dialogue conditions read the loaded values and not
  * the previous session's. Text-exported with the CPP type alongside, same as BPLM progress.
  */
 UPROPERTY()
 TMap<FName, FString> GameInstanceValues;

 UPROPERTY()
 TMap<FName, FString> GameInstanceTypes;
};

/** Tiny companion file holding only FVTGSlotMeta, so the load menu can enumerate slots cheaply. */
UCLASS()
class VERTIGO_API UVTGSlotManifest : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FVTGSlotMeta Meta;
};
