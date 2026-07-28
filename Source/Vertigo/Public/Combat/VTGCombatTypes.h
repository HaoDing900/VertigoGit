#pragma once

#include "CoreMinimal.h"
#include "VTGCombatTypes.generated.h"

/**
 * How the victim should react to a hit. This replaces the enemy's single "Uni Hit Montage" - the
 * attack data asset picks the tier, the victim's combat component picks the montage for that tier,
 * so light jabs and finishers no longer look identical.
 */
UENUM(BlueprintType)
enum class EVTGHitReact : uint8
{
	/** No reaction at all - chip damage, poison ticks, etc. Victim keeps doing whatever it was doing. */
	None		UMETA(DisplayName = "None"),
	/** Small flinch. Combo filler. */
	Light		UMETA(DisplayName = "Light"),
	/** Big flinch + pushback. Combo enders. */
	Heavy		UMETA(DisplayName = "Heavy"),
	/** Long stun, opens the victim for a Heat Action / grab. */
	Stagger		UMETA(DisplayName = "Stagger"),
	/** Sends the victim airborne (juggle). */
	Launch		UMETA(DisplayName = "Launch"),
	/** Puts the victim on the floor - eligible for stomp / ground pickup. */
	Knockdown	UMETA(DisplayName = "Knockdown"),
	/** The hit was blocked. Small guard flinch, reduced damage. Set by the victim, not the attacker. */
	Guard		UMETA(DisplayName = "Guard")
};

/**
 * The ONE state variable that replaces DuringPunch? / CanAttack? / CanPunch? / IsAiming / IsDead? /
 * IsInCineCutscene_LockInput / Crowd Control State. Everything asks UVTGCombatComponent::CanDoAction
 * instead of chaining Branch nodes, so "died mid-punch" and "punched during a cutscene" stop being possible.
 */
UENUM(BlueprintType)
enum class EVTGCombatState : uint8
{
	/** Free. Can do anything. */
	Idle		UMETA(DisplayName = "Idle"),
	/** Attack montage is playing. Only combo-window input and (optionally) dodge-cancel get through. */
	Attacking	UMETA(DisplayName = "Attacking"),
	/** Taking a hit reaction. No input. */
	HitStun		UMETA(DisplayName = "HitStun"),
	/** Holding guard. */
	Blocking	UMETA(DisplayName = "Blocking"),
	/** I-frames / dodge roll. */
	Dodging		UMETA(DisplayName = "Dodging"),
	/** Holding an enemy (or being the one doing a Heat Action). */
	Grabbing	UMETA(DisplayName = "Grabbing"),
	/** Being held by someone else. */
	Grabbed		UMETA(DisplayName = "Grabbed"),
	/** On the floor. Getting up, or eligible to be stomped. */
	Downed		UMETA(DisplayName = "Downed"),
	/** Dead. Absorbing state - nothing leaves it. */
	Dead		UMETA(DisplayName = "Dead"),
	/** Cutscene / dialogue / sequencer owns this actor. Absorbing until explicitly cleared. */
	Cinematic	UMETA(DisplayName = "Cinematic")
};

/**
 * Every verb the player or an AI can try to start. Input entry points (IA_Fire, IA_Punch, IA_Dodge...)
 * call CanDoAction(...) once with one of these instead of testing a pile of bools.
 */
UENUM(BlueprintType)
enum class EVTGCombatAction : uint8
{
	Attack		UMETA(DisplayName = "Attack"),
	HeavyAttack	UMETA(DisplayName = "Heavy Attack"),
	Block		UMETA(DisplayName = "Block"),
	Dodge		UMETA(DisplayName = "Dodge"),
	Grab		UMETA(DisplayName = "Grab"),
	HeatAction	UMETA(DisplayName = "Heat Action"),
	Fire		UMETA(DisplayName = "Fire"),
	Aim			UMETA(DisplayName = "Aim"),
	Move		UMETA(DisplayName = "Move"),
	Jump		UMETA(DisplayName = "Jump"),
	Interact	UMETA(DisplayName = "Interact")
};

/**
 * The single currency of the combat system. Player fists, enemy fists, thrown bikes, bullets,
 * hazards - all of them build one of these and hand it to IVTGDamageable::ReceiveCombatHit.
 *
 * The point: the ATTACKER owns the whole description of the hit (how much it hurts, how the victim
 * reacts, how far it throws them, how long BOTH sides freeze). That's why hitstop finally matches on
 * both actors - one number, applied to both, from one place.
 */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGHitEvent
{
	GENERATED_BODY()

	/** Raw damage before the victim's own modifiers (block, armour, difficulty scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Damage = 10.f;

	/** Which reaction tier the victim should play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	EVTGHitReact ReactType = EVTGHitReact::Light;

	/**
	 * Knockback / launch impulse, in cm/s, fed to LaunchCharacter.
	 * By default it's expressed in the ATTACKER's space (X = away from attacker, Z = up), so one
	 * attack asset works no matter which way the fight is facing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FVector Launch = FVector::ZeroVector;

	/** True: Launch is attacker-relative (normal case). False: Launch is already world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bLaunchIsAttackerRelative = true;

	/** Seconds of freeze applied to BOTH actors on impact. 0 = no hitstop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitStop = 0.08f;

	/** How long the victim is locked in HitStun after the reaction starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitStunDuration = 0.35f;

	/** False for unblockable moves (Heat Actions, boss specials). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bBlockable = true;

	/** Who threw the punch. Used for facing, team checks, and "who killed me" reporting. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TObjectPtr<AActor> Instigator = nullptr;

	/**
	 * Id of the attack that produced this hit (usually UVTGAttackDataAsset::AttackId).
	 * Use this instead of the old string compares like `EqualEqual_StrStr "SMG1_May"`.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	FName AttackId = NAME_None;

	/** World-space impact point - where the VFX/SFX go. Filled in by the trace. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	FVector HitLocation = FVector::ZeroVector;

	/** Impact normal, for orienting the impact VFX. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	FVector HitNormal = FVector::ZeroVector;

	/** Bone that was hit, when the trace found one. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	FName HitBone = NAME_None;
};
