#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/VTGCombatTypes.h"
#include "VTGAttackData.generated.h"

class UAnimMontage;

/**
 * One asset = one move. This is what kills the three hardcoded `Set Anim_Punch` nodes: a combo is
 * just a chain of these assets, so adding a 4th punch means creating a 4th asset, not editing a
 * Blueprint graph. Every move gets its own damage / reaction / launch / hitstop / cancel window.
 *
 * NOTE: this is the minimum version needed to make UVTGCombatComponent work. Heat, styles and
 * weapon moves get added here later (step 2 of the plan) without touching the component.
 */
UCLASS(BlueprintType)
class VERTIGO_API UVTGAttackDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Stable id stamped into FVTGHitEvent::AttackId. Use it for VFX/SFX lookups and combo counters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName AttackId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float PlayRate = 1.f;

	// --- What it does on hit -------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	float Damage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	EVTGHitReact HitReactType = EVTGHitReact::Light;

	/** Attacker-relative: +X pushes the victim away from you, +Z lifts them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	FVector LaunchImpulse = FVector::ZeroVector;

	/** Seconds both actors freeze on impact. Bigger = heavier. Combo enders ~0.12, jabs ~0.05. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	float HitStopDuration = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	float HitStunDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	bool bBlockable = true;

	/** Sockets the melee trace sweeps between/around during the active frames (e.g. hand_r, lowerarm_r). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Trace")
	TArray<FName> TraceSockets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Trace")
	float TraceRadius = 40.f;

	// --- Combo ---------------------------------------------------------------------------------

	/**
	 * What this move can flow into. Empty = combo ender.
	 * Multiple entries = branching combo (light/heavy follow-ups); the component picks the first
	 * whose input matches, so ordering is priority.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	TArray<TObjectPtr<UVTGAttackDataAsset>> NextAttacks;

	/** Which button continues the combo into NextAttacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	EVTGCombatAction ContinueAction = EVTGCombatAction::Attack;

	/**
	 * Fallback combo window, in seconds from montage start, used only when bUseTimedComboWindow is
	 * true. The real system is ANS_Melee_ComboWindow calling Open/CloseComboWindow (step 5); these
	 * timers exist so combos are testable before the notify states are authored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	bool bUseTimedComboWindow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo", meta = (EditCondition = "bUseTimedComboWindow"))
	float ComboWindowStart = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo", meta = (EditCondition = "bUseTimedComboWindow"))
	float ComboWindowEnd = 0.7f;

	/** Allow dodge/block to cancel out of this move once the combo window is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	bool bCanCancelInto = true;

	// --- Movement / cost -----------------------------------------------------------------------

	/** Freeze the character's movement mode for the duration (mirrors the old DisableMovement). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Movement")
	bool bLockMovement = true;

	/** Heat spent to use the move (0 = free). Heat itself lands later; the field is here so data doesn't churn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Heat")
	float HeatCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Heat")
	float HeatGainOnHit = 0.f;
};
