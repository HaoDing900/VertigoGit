#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTGCheckpoint.generated.h"

class UArrowComponent;
class UBoxComponent;

/**
 * Drop into a level where the player should respawn after dying.
 *
 * When the player walks into the box, the game autosaves as this checkpoint; "Retry" on the death
 * screen then reloads that save and puts the player on the ARROW (position + facing), not wherever
 * they were standing when they crossed the box.
 *
 *  - Order: give later checkpoints a higher number. Walking back into an earlier one does nothing.
 *  - Required Stage: leave empty to work in every stage of the level.
 *  - Already-passed checkpoints don't save again when the level is restored from a later one.
 *
 * For a checkpoint at a story beat instead of a place (end of a cutscene, start of a fight),
 * call VTG Save Coordinator -> Save Checkpoint from the BPLM.
 */
UCLASS(Blueprintable)
class VERTIGO_API AVTGCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AVTGCheckpoint();

	/** Unique within the level; BPLM reads it back from Get Resume Checkpoint. Empty = actor name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId;

	/** Progress order within the level. Higher = further along. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	int32 Order = 0;

	/** Only active in this stage. Empty = every stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	FName RequiredStage;

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FName GetCheckpointId() const { return CheckpointId.IsNone() ? GetFName() : CheckpointId; }

	/** Override to block saving at awkward moments (mid-fight, during a scripted beat...). */
	UFUNCTION(BlueprintNativeEvent, Category = "Checkpoint")
	bool CanActivate(APawn* Player) const;

	/** Fired after the checkpoint saved - hook a "Checkpoint" toast / sound here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnCheckpointReached(APawn* Player);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UBoxComponent> Trigger;

	/** Where (and facing which way) the player respawns. Move/rotate it in the viewport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UArrowComponent> RespawnPoint;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	bool bActivated = false;
};
