#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGParallaxScrollComponent.generated.h"

/**
 * One parallax layer: a set of leapfrogging actor "groups" (A, B, optionally C...) that scroll
 * along a direction and wrap around to fake an endless backdrop (e.g. the offshore-rig skyline).
 *
 * The groups are actors YOU place in the level - typically two copies of the same layer BP, set one
 * tile-width apart along -Direction. At runtime each group slides along Direction and, once it passes
 * the front of the conveyor, teleports one full loop-length back behind the others. Because every group
 * wraps against the same window, they leapfrog cleanly and you never hand-tune "when A exits, move B".
 */
USTRUCT(BlueprintType)
struct FVTGScrollLayer
{
	GENERATED_BODY()

	/** Optional label just so the two entries read as "Foreground"/"Background" in the details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FName LayerName = TEXT("Layer");

	/**
	 * The groups for this layer, in conveyor order:
	 *   Index [0] = group A  <- THE ANCHOR. Its position is the layer's start point.
	 *   Index [1] = group B, Index [2] = group C, ... (as many as you want)
	 *
	 * Only A's position matters; B/C/... are auto-spaced TileWidth apart at BeginPlay, so you can drop
	 * them anywhere (off screen is fine) and the component snaps them into the conveyor.
	 *
	 * Fill rules (handled at BeginPlay):
	 *   - Any empty/None or invalid slot is skipped - a "hole" won't break anything.
	 *   - Extra valid entries (C, D, ...) are all used, evenly spaced.
	 *   - If you leave only A filled (B is None), the component auto-duplicates A into a second group
	 *     for you, so a 2-tile loop still works without hand-placing B.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	TArray<TObjectPtr<AActor>> Groups;

	/** Scroll direction in world space (will be normalized). Default (0, -1, 0) travels along -Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FVector Direction = FVector(0.f, -1.f, 0.f);

	/** Scroll speed in cm/s along Direction. Foreground fast (e.g. 0.5x base), background slow (0.1x). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	float Speed = 200.f;

	/**
	 * Width of one tile = the spacing between adjacent groups, in cm along Direction.
	 * This is the ONLY spacing knob - groups are laid out this far apart and wrap by TileWidth x count.
	 * Leave 0 to auto-use the first group's mesh bounds width. Set it by hand for a seamless period.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	float TileWidth = 0.f;

	// --- runtime, filled in at BeginPlay ---
	FVector DirUnit = FVector(0.f, -1.f, 0.f);
	FVector AnchorPerp = FVector::ZeroVector;  // first group's position, perpendicular part (kept fixed)
	float AnchorAlong = 0.f;                    // first group's projection onto DirUnit (the start point)
	float ResolvedTile = 0.f;                   // TileWidth after auto-measure
	float Loop = 0.f;                           // ResolvedTile x Groups.Num()
	float WindowStart = 0.f;                    // back edge of the wrap window
	float Accum = 0.f;                          // shared distance scrolled so far
};

/**
 * Drop-in endless-parallax driver. Add it to any actor (a level "director" BP is fine), then fill the
 * Layers array with one entry for your Foreground and one for your Background. Each layer scrolls and
 * loops independently, so the fast foreground and slow background cycle at their own rates for free.
 *
 * Tie every Speed to one base value in your own code/BP if you want a single "slow to a stop" knob:
 * set each layer's Speed = BaseSpeed x multiplier and lerp BaseSpeed to 0.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGParallaxScrollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTGParallaxScrollComponent();

	/** One entry per parallax layer. Typically two: [0] Foreground, [1] Background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parallax")
	TArray<FVTGScrollLayer> Layers;

	/** Master on/off. Also stops when every layer has 0 usable groups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parallax")
	bool bScrolling = true;

	/** Set a layer's speed at runtime by index (e.g. from a "vehicle slowing down" curve). */
	UFUNCTION(BlueprintCallable, Category = "Parallax")
	void SetLayerSpeed(int32 LayerIndex, float NewSpeed);

	/** Scale every layer's speed at once - lerp this to 0 to bring the whole backdrop to a stop. */
	UFUNCTION(BlueprintCallable, Category = "Parallax")
	void SetGlobalSpeedScale(float Scale);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void InitLayer(FVTGScrollLayer& Layer);
	void TickLayer(FVTGScrollLayer& Layer, float DeltaTime);

	/** Runtime clone of a group actor (same class + properties), used to fill in a missing group B. */
	AActor* DuplicateGroup(AActor* Source) const;

	/** Speeds captured at BeginPlay so SetGlobalSpeedScale can scale against the original values. */
	TArray<float> BaseSpeeds;
};
