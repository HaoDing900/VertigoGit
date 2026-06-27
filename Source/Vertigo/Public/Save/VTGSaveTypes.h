#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VTGSaveTypes.generated.h"

/**
 * Bump this whenever the saved data layout changes in a way old saves can't be read 1:1.
 * The coordinator stamps it into every save and you can branch on it in a migration step later.
 * Saving this from day one is the difference between "migrate old saves" and "every playtest is bricked".
 */
#define VTG_SAVE_VERSION 1

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

/**
 * The full save payload for one slot - everything Vertigo's own code owns. Narrative and ISX keep
 * their OWN files (the coordinator writes them to matching slots); this object is the glue state:
 * player status + level-placed actor state + the slot meta.
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

	UPROPERTY()
	bool bHasPlayerTransform = false;

	UPROPERTY()
	FTransform PlayerTransform = FTransform::Identity;

	/** Level-placed stateful actors (BPLM children, doors, taken pickups...), keyed by Save Id. */
	UPROPERTY()
	TMap<FName, FVTGActorRecord> ActorRecords;

	/** Checkpoint flags restored BEFORE the map opens, so BeginPlay can branch on them (ints/enums/bools). */
	UPROPERTY()
	TMap<FName, int32> PersistentInts;

	/** Same, for name-valued checkpoint state. */
	UPROPERTY()
	TMap<FName, FName> PersistentNames;
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
