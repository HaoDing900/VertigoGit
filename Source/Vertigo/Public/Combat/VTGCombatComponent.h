#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h" // FBranchingPointNotifyPayload
#include "Kismet/KismetSystemLibrary.h" // EDrawDebugTrace
#include "Combat/VTGCombatTypes.h"
#include "VTGCombatComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;
class UVTGAttackDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVTGOnCombatStateChanged, EVTGCombatState, OldState, EVTGCombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVTGOnHitLanded, AActor*, Target, const FVTGHitEvent&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGOnHitReceived, const FVTGHitEvent&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVTGOnHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGOnDeath, AActor*, Killer);

/**
 * THE combat brain. Player and enemies both use this one component, so the attack flow exists once
 * instead of twice (player SphereTrace + ApplyDamage vs enemy CollisionComponent + ProcessDamage).
 *
 * What it owns:
 *   - the single combat state (replaces DuringPunch? / CanAttack? / IsDead? / cutscene lock / ...)
 *   - starting attacks from UVTGAttackDataAsset, and chaining combos through NextAttacks
 *   - input buffering, so a press 0.05s early is remembered instead of eaten
 *   - the melee trace (continuous sweep between frames + per-swing hit dedupe)
 *   - taking hits: block check, health, reaction montage, launch, hitstun, death
 *   - hitstop, driven by the ATTACKER onto BOTH actors so the freeze finally matches
 *
 * Blueprint wiring (typical):
 *   Input "Punch"            -> TryAction(Attack)
 *   ANS_Melee_Trace  Begin   -> BeginMeleeTrace / Tick -> TickMeleeTrace / End -> EndMeleeTrace
 *   ANS_ComboWindow  Begin   -> OpenComboWindow  / End -> CloseComboWindow
 *   OnHitLanded / OnHitReceived -> spawn Niagara + sound + camera shake (kept in BP on purpose:
 *   no Niagara dependency in this module, and VFX is where designers iterate).
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTGCombatComponent();

	// =========================================================================================
	//  State
	// =========================================================================================

	/** The single gate. Every input entry point calls this ONCE instead of chaining Branch nodes. */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool CanDoAction(EVTGCombatAction Action) const;

	UFUNCTION(BlueprintPure, Category = "Combat|State")
	EVTGCombatState GetCombatState() const { return CombatState; }

	/**
	 * Force a state. Use for the states this component doesn't drive itself - mainly Cinematic
	 * (sequencer/dialogue takes over) and Grabbed. Attacking/HitStun/Dead are managed internally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	void SetCombatState(EVTGCombatState NewState);

	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsAlive() const { return CombatState != EVTGCombatState::Dead && CurrentHealth > 0.f; }

	// =========================================================================================
	//  Attacking
	// =========================================================================================

	/**
	 * Input entry point. Returns true if the action started right now.
	 * If it can't start yet but might in a moment (mid-attack, before the combo window opens),
	 * the press is BUFFERED and fires automatically when the window opens - that's the fix for
	 * the old `Delay 0.3` swallowing early inputs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	bool TryAction(EVTGCombatAction Action);

	/** Start a specific move immediately, skipping gating. For AI, scripted beats and Heat Actions. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	bool StartAttack(UVTGAttackDataAsset* Attack);

	/** Abort the current attack (interrupt, death, cutscene). Restores movement, clears the trace. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void CancelAttack();

	/** Called by ANS_ComboWindow (or the fallback timer). While open, the next press chains instantly. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void OpenComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void CloseComboWindow();

	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	UVTGAttackDataAsset* GetCurrentAttack() const { return CurrentAttack; }

	// =========================================================================================
	//  Melee trace  (replaces the two copy-pasted SphereTraceSingleForObjects graphs)
	// =========================================================================================

	/** Open the hitbox. Sockets/radius default to the current attack's when left empty / <= 0. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	void BeginMeleeTrace(const TArray<FName>& Sockets, float Radius = 0.f);

	/**
	 * Sweep since the previous tick. Driven automatically by this component's own tick while the
	 * hitbox is open, so no notify state is needed for it - but it's exposed in case you want to
	 * drive it from an AnimNotifyState later.
	 *
	 * Sweeping from last frame's socket position (not a single frame-snapshot trace) is what stops
	 * fast punches from tunnelling through a target.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	void TickMeleeTrace();

	/** Close the hitbox and clear the per-swing dedupe list. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	void EndMeleeTrace();

	/** Build the hit event for the current attack and send it through IVTGDamageable. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	bool ApplyHitTo(AActor* Target, const FHitResult& HitResult);

	// =========================================================================================
	//  Taking damage
	// =========================================================================================

	/** Point this at IVTGDamageable::ReceiveCombatHit on the owner. Does block/health/react/death. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage")
	void HandleIncomingHit(const FVTGHitEvent& Hit);

	/**
	 * Migration bridge: wrap a legacy float damage (ReceiveAnyDamage / ApplyDamage from old BP
	 * graphs and the AI Behavior plugin) into a proper FVTGHitEvent so old and new coexist while
	 * BP_Player_Sa and BP_Enm_BarFighter are being converted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage")
	void HandleLegacyDamage(float Damage, AActor* Causer);

	UFUNCTION(BlueprintCallable, Category = "Combat|Damage")
	void Heal(float Amount);

	UFUNCTION(BlueprintPure, Category = "Combat|Damage")
	float GetCurrentHealth() const { return CurrentHealth; }

	/** Kill immediately (scripted deaths, falls, cutscene beats). */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage")
	void Kill(AActor* Killer);

	/** Freeze an actor for a moment. Static so the attacker can drive it on BOTH sides of the hit. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage")
	static void ApplyHitStop(AActor* Actor, float Duration, float TimeDilation = 0.05f);

	// =========================================================================================
	//  Config
	// =========================================================================================

	/** The default combo: entry move for TryAction(Attack). Each move chains on via NextAttacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	TObjectPtr<UVTGAttackDataAsset> DefaultAttack;

	/** Entry move for TryAction(HeavyAttack). Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	TObjectPtr<UVTGAttackDataAsset> DefaultHeavyAttack;

	/** How long an early press stays remembered. 0.25 is the usual brawler feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	float InputBufferWindow = 0.25f;

	/** Snap-turn toward the nearest valid target when an attack starts (Yakuza-style soft lock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	bool bFaceTargetOnAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack", meta = (EditCondition = "bFaceTargetOnAttack"))
	float SoftLockRadius = 400.f;

	/** Half-angle of the soft-lock cone, degrees. 60 = a 120-degree wedge in front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack", meta = (EditCondition = "bFaceTargetOnAttack"))
	float SoftLockHalfAngle = 60.f;

	/** Hit reaction montages per tier. Set at least Light; missing tiers fall back to Light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Reactions")
	TMap<EVTGHitReact, TObjectPtr<UAnimMontage>> HitReactMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
	float MaxHealth = 100.f;

	/**
	 * Friendly-fire gate. Two actors with the SAME TeamId (and >= 0) can't hit each other - use it
	 * to stop a crowd of enemies punching each other apart during a brawl.
	 * Default -1 means "no team": hits everyone, gets hit by everyone. Suggested: player 0, enemies 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
	int32 TeamId = -1;

	/** Damage multiplier while Blocking and the hit came from the front. 0 = perfect guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Reactions")
	float BlockDamageMultiplier = 0.2f;

	/** Half-angle of the guard arc, degrees. Hits from outside it ignore the block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Reactions")
	float BlockHalfAngle = 70.f;

	// =========================================================================================
	//  Montage notify hookup
	// =========================================================================================
	//
	// The punch montages ALREADY carry a "Montage Notify Window" (BossAIToolkit's Notify_Damage,
	// which derives from UAnimNotify_PlayMontageNotifyWindow). Those broadcast
	// UAnimInstance::OnPlayMontageNotifyBegin/End, which we bind here - so the hitbox timing that
	// was authored on those montages keeps working and no new notify assets are needed.
	//
	// Heads-up: on the current Sa punch montages the notify's Name field was left blank, so the
	// broadcast name is None. That's why DamageNotifyName defaults to None = "match anything".

	/**
	 * Which notify window opens the hitbox. Leave as None to treat ANY notify window that fires
	 * during an attack as the damage window (what the current montages need).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Notifies")
	FName DamageNotifyName = NAME_None;

	/**
	 * Which notify window opens the combo/cancel window. Only used once you actually name a notify
	 * this; until then the timed fallback on the attack asset does the job.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Notifies")
	FName ComboNotifyName = TEXT("Combo");

	/** Turn off to ignore montage notifies entirely and drive traces from your own Blueprint calls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Notifies")
	bool bBindMontageNotifies = true;

	/** Object types the melee trace looks for. Defaults to Pawn + WorldDynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Trace")
	TEnumAsByte<EDrawDebugTrace::Type> TraceDebugDraw = EDrawDebugTrace::None;

	/**
	 * Log every attack start/end, hitbox open/close and montage notify to the Output Log.
	 * Filter the log with "VTGCombat" - the sequence tells you exactly where a stuck attack died.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Trace")
	bool bDebugLog = false;

	// =========================================================================================
	//  Events  (VFX / SFX / UI hang off these in Blueprint)
	// =========================================================================================

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FVTGOnCombatStateChanged OnCombatStateChanged;

	/** We hit someone. Spawn impact VFX/SFX, camera shake, bump the combo counter. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FVTGOnHitLanded OnHitLanded;

	/** We got hit (fires even if blocked - check Hit.ReactType == Guard). */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FVTGOnHitReceived OnHitReceived;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FVTGOnHealthChanged OnHealthChanged;

	/** Ragdoll, loot, BPI_OnEnmDeath cutscene hook - all of that stays in Blueprint, hung off here. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FVTGOnDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Bound to UAnimInstance::OnPlayMontageNotifyBegin - fired by any "Montage Notify Window". */
	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	UFUNCTION()
	void HandleMontageNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

private:
	// --- runtime state -------------------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Debug")
	EVTGCombatState CombatState = EVTGCombatState::Idle;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Debug")
	float CurrentHealth = 0.f;

	UPROPERTY()
	TObjectPtr<UVTGAttackDataAsset> CurrentAttack = nullptr;

	/** True between OpenComboWindow and CloseComboWindow. */
	bool bComboWindowOpen = false;

	/** Pending input, and when it was pressed (world seconds). */
	EVTGCombatAction BufferedAction = EVTGCombatAction::Attack;
	bool bHasBufferedAction = false;
	float BufferedActionTime = 0.f;

	/** Set while we deliberately interrupt our own montage to chain - stops the end-delegate resetting state. */
	bool bChainingCombo = false;

	// --- trace state ---------------------------------------------------------------------------

	bool bTraceActive = false;
	TArray<FName> ActiveTraceSockets;
	float ActiveTraceRadius = 40.f;

	/** Previous frame's socket positions, so each tick sweeps a capsule instead of a point. */
	TMap<FName, FVector> PreviousSocketLocations;

	/** One hit per actor per swing. Cleared by BeginMeleeTrace. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisSwing;

	// --- timers --------------------------------------------------------------------------------

	FTimerHandle ComboWindowOpenTimer;
	FTimerHandle ComboWindowCloseTimer;
	FTimerHandle HitStunTimer;

	/** Forces cleanup if the montage's end callback never arrives. See AttackWatchdog(). */
	FTimerHandle AttackWatchdogTimer;

	// --- helpers -------------------------------------------------------------------------------

	/** Alive + not on our own team + not ourselves. Reads the target's component, not an interface. */
	bool IsValidTarget(AActor* Target) const;

	UAnimInstance* GetOwnerAnimInstance() const;
	USkeletalMeshComponent* GetOwnerMesh() const;

	/** Subscribe to the anim instance's montage-notify delegates. Safe to call repeatedly. */
	void BindMontageNotifies();
	void UnbindMontageNotifies();

	/** The anim instance we're currently subscribed to, so we can unbind from the right one. */
	UPROPERTY()
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;

	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void FinishAttack();

	/** Safety net: never let a missing montage callback leave the actor locked mid-attack. */
	void AttackWatchdog();

	void DebugLog(const FString& Message) const;

	/** Wiring mistakes: always logged AND shown on screen, even with Debug Log off. */
	void ReportCombatProblem(const FString& Message) const;

	/** Consume a fresh buffered press, if any. Called when the combo window opens. */
	bool TryConsumeBufferedAction();

	/** Pick the follow-up move for Action from CurrentAttack->NextAttacks. */
	UVTGAttackDataAsset* ResolveNextAttack(EVTGCombatAction Action) const;

	/** Entry move for an action when we're not already in a combo. */
	UVTGAttackDataAsset* ResolveEntryAttack(EVTGCombatAction Action) const;

	void SetMovementLocked(bool bLocked);
	void FaceBestTarget();
	AActor* FindSoftLockTarget() const;

	void PlayHitReact(const FVTGHitEvent& Hit);
	void EnterHitStun(float Duration);
	void ExitHitStun();
	void HandleDeath(AActor* Killer);
};
