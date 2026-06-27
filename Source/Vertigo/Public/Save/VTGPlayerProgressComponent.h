#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGPlayerProgressComponent.generated.h"

/**
 * One home for the player's persistent status. Put every value that must survive a save here and
 * tick it "SaveGame" (already done for the ones below). The SaveCoordinator serialises this whole
 * component in a single call, so you never touch the save code when you add a new stat - just add a
 * SaveGame property here and it is persisted automatically.
 *
 * Add this component to your player Pawn (or PlayerState). The Counters / Flags maps let you record
 * arbitrary progress ("BarKeeperBribed" = true, "TimesDied" = 3) without a code change at all.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGPlayerProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVTGPlayerProgressComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player")
	float MaxHealth = 100.f;

	/** Free-form integer progress (counters, currencies...) keyed by name - add keys without code. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player")
	TMap<FName, int32> Counters;

	/** Free-form boolean progress (one-off events done, doors unlocked...) keyed by name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player")
	TMap<FName, bool> Flags;
};
