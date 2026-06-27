#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VTGSaveable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UVTGSaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Add this interface to any actor whose state must survive save/load - BPLM children, doors,
 * switches, pickups the player has already taken, etc. The SaveCoordinator finds every actor in the
 * level that implements this and automatically serialises the variables you have ticked "SaveGame".
 *
 * The zero-code path is:
 *   1. Add this interface to the actor (Class Settings -> Implemented Interfaces).
 *   2. Tick "SaveGame" on the variables you want persisted.
 * That's it - no nodes needed. The two functions below are OPTIONAL overrides:
 *   - GetSaveId : override only if the actor's name isn't a stable key (e.g. it's spawned at runtime).
 *                 Leave it and the coordinator uses the actor's name automatically.
 *   - ShouldSaveTransform : override and return true if the actor also moves and you want its
 *                 position/rotation restored.
 */
class VERTIGO_API IVTGSaveable
{
	GENERATED_BODY()

public:

	/** Optional stable key. Default (NAME_None) tells the coordinator to use the actor's own name. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	FName GetSaveId() const;

	/** Return true to also save/restore this actor's world transform. Default: false. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	bool ShouldSaveTransform() const;
};
