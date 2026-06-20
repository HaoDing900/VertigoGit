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
};
