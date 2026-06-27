#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGLevelStatics.generated.h"

/**
 * Convenience helpers for the stage system. Stage is a plain Name throughout.
 */
UCLASS()
class VERTIGO_API UVTGLevelStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * The level's current stage Name.
	 *
	 * Order-independent: works correctly from any BeginPlay, no matter whether the level manager
	 * has run yet. Prefers a level manager's test override (so in-editor testing works), and
	 * otherwise falls back to the persistent GameInstance value.
	 */
	UFUNCTION(BlueprintPure, Category = "Stage", meta = (WorldContext = "WorldContextObject"))
	static FName GetActiveStage(const UObject* WorldContextObject);

	/**
	 * True if the level's current stage matches StageToCheck.
	 *
	 * This is the one-node check for "should I exist here?". In SmartTrigger / ISX Collision /
	 * Camera Lock etc, feed in that actor's own stage Name; if it returns false, destroy yourself.
	 * An empty StageToCheck means "exists in every stage" (never destroyed by this check).
	 */
	UFUNCTION(BlueprintPure, Category = "Stage", meta = (WorldContext = "WorldContextObject"))
	static bool IsActiveStage(const UObject* WorldContextObject, FName StageToCheck);

	/**
	 * Opens a level and, at the same time, tells the next level which stage to start in.
	 *
	 * If Stage is left empty it is ignored - the level just opens with whatever stage is already set.
	 * Drop-in replacement for "Open Level (by Name)" that also carries the stage across.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bAbsolute,Options"))
	static void OpenLevelWithStage(const UObject* WorldContextObject, FName LevelName, FName Stage, bool bAbsolute = true, const FString& Options = FString(TEXT("")));

	/**
	 * Reliable replacement for "Get Actor Of Class". Returns the first actor of the class that is fully valid and
	 * NOT being destroyed. Skips actors that are mid-destruction (e.g. stage-gated actors removing themselves),
	 * which is the usual reason "Get Actor Of Class" intermittently returns nothing useful.
	 */
	UFUNCTION(BlueprintPure, Category = "Utility", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass"))
	static AActor* FindValidActorOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass);

	/**
	 * Same as FindValidActorOfClass, but also requires the actor to carry the given Tag. Use this to pick the RIGHT
	 * one when several actors of the same class exist (tag your correct, in-scene actor). This is the bullet-proof
	 * way to grab a specific camera point / character regardless of duplicates or destruction order.
	 */
	UFUNCTION(BlueprintPure, Category = "Utility", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass"))
	static AActor* FindValidActorOfClassWithTag(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FName Tag);

	/**
	 * Finds the first valid actor of a class in the SAME WORLD as WorldSource - instead of using the calling
	 * object's own world context. Use this inside Narrative Events and feed the event's NARRATIVE COMPONENT pin
	 * into WorldSource (it is always valid, unlike Pawn/Controller which can be null on a non-player dialogue).
	 * Accepts any object (Component, Pawn, Controller, Actor...).
	 */
	UFUNCTION(BlueprintPure, Category = "Utility", meta = (DeterminesOutputType = "ActorClass"))
	static AActor* FindActorOfClassInWorldOf(const UObject* WorldSource, TSubclassOf<AActor> ActorClass);
};
