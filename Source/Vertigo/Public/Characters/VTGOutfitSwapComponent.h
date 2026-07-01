#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "VTGOutfitSwapComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;

/**
 * Drop-in outfit swapper for the toon characters (Sa, etc.).
 *
 * The character is one body skeletal-mesh component plus an inverted-hull "outline" component that
 * shares the body's mesh. Swapping a costume is therefore just swapping the same SkeletalMesh asset
 * onto BOTH components - the trick is materials:
 *   - the body inherits its materials from the mesh ASSET (we clear any component overrides), and
 *   - the outline forces every slot to OutlineMaterial (MM_Outline_01a_size15 by default).
 *
 * Setup: add this component to the character, then in the details panel pick the Body Mesh and
 * Outline Mesh components. Call SwapOutfit(NewBody) from a custom event whenever the costume changes.
 * All costume meshes must share the same skeleton so the AnimBP and Leader Pose keep working.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGOutfitSwapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTGOutfitSwapComponent();

	/** The main body skeletal mesh component (e.g. the Character's "Mesh"). Pick it in the details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit",
		meta = (UseComponentPicker, AllowedClasses = "SkeletalMeshComponent"))
	FComponentReference BodyMesh;

	/** The outline shell component that shares the body's mesh (e.g. "CharacterOutline"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit",
		meta = (UseComponentPicker, AllowedClasses = "SkeletalMeshComponent"))
	FComponentReference OutlineMesh;

	/**
	 * Per-slot outline materials (slot 0 = face uses size15, others size35/70, etc.).
	 * Leave EMPTY to auto-capture whatever the outline component is configured with at BeginPlay
	 * (recommended - it just inherits the defaults you set up in the editor). These are re-applied
	 * slot-by-slot after every swap so each body region keeps its own outline thickness.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	TArray<TObjectPtr<UMaterialInterface>> OutlineMaterials;

	/** Fallback outline material for any slot a new costume adds beyond the captured list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	TObjectPtr<UMaterialInterface> DefaultOutlineMaterial;

	/** Re-point the outline's Leader Pose at the body after each swap so it stays synced (avoids T-pose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	bool bRebindLeaderPose = true;

	/**
	 * Swap both body and outline to NewBody.
	 * Body falls back to the asset's own materials; outline gets OutlineMaterial on all slots.
	 * Trigger this during a "getting dressed" animation or behind a fade to hide the 1-frame pose reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Outfit")
	void SwapOutfit(USkeletalMesh* NewBody);

	/** Re-apply the per-slot outline materials onto the outline component. Called by SwapOutfit; exposed for manual fixes. */
	UFUNCTION(BlueprintCallable, Category = "Outfit")
	void RefreshOutlineMaterials();

protected:
	virtual void BeginPlay() override;

private:
	USkeletalMeshComponent* ResolveBody() const;
	USkeletalMeshComponent* ResolveOutline() const;
};
