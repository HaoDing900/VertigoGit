#include "Combat/VTGCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/VTGAttackData.h"
#include "Combat/VTGDamageable.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

UVTGCombatComponent::UVTGCombatComponent()
{
	// Everything else is event/notify/timer driven; the only thing that needs a real tick is the
	// melee sweep, and only for the handful of frames the hitbox is open. So: tick is allowed, but
	// starts off and is switched on by BeginMeleeTrace / off by EndMeleeTrace.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
}

void UVTGCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	BindMontageNotifies();
}

void UVTGCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMontageNotifies();
	Super::EndPlay(EndPlayReason);
}

void UVTGCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTraceActive)
	{
		TickMeleeTrace();
	}
	else
	{
		// Nothing to sweep - go back to sleep rather than burning a tick per actor per frame.
		SetComponentTickEnabled(false);
	}
}

// =================================================================================================
//  Montage notify hookup
// =================================================================================================

void UVTGCombatComponent::BindMontageNotifies()
{
	if (!bBindMontageNotifies)
	{
		return;
	}

	UAnimInstance* Anim = GetOwnerAnimInstance();
	if (!Anim || BoundAnimInstance.Get() == Anim)
	{
		return; // nothing to bind to yet, or already bound to this one
	}

	UnbindMontageNotifies();

	Anim->OnPlayMontageNotifyBegin.AddDynamic(this, &UVTGCombatComponent::HandleMontageNotifyBegin);
	Anim->OnPlayMontageNotifyEnd.AddDynamic(this, &UVTGCombatComponent::HandleMontageNotifyEnd);
	BoundAnimInstance = Anim;
}

void UVTGCombatComponent::UnbindMontageNotifies()
{
	if (UAnimInstance* Anim = BoundAnimInstance.Get())
	{
		Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UVTGCombatComponent::HandleMontageNotifyBegin);
		Anim->OnPlayMontageNotifyEnd.RemoveDynamic(this, &UVTGCombatComponent::HandleMontageNotifyEnd);
	}
	BoundAnimInstance = nullptr;
}

void UVTGCombatComponent::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	DebugLog(FString::Printf(TEXT("notify BEGIN '%s' (state=%d)"), *NotifyName.ToString(), (int32)CombatState));

	// Only attack montages get to open a hitbox - a notify window on a dodge or a reaction
	// (Notify_Sa_DodgeInvincible, say) must not start a trace.
	if (CombatState != EVTGCombatState::Attacking)
	{
		return;
	}

	if (!ComboNotifyName.IsNone() && NotifyName == ComboNotifyName)
	{
		OpenComboWindow();
		return;
	}

	// DamageNotifyName == None means "any window is the damage window", which is what the existing
	// Sa punch montages need - their Notify_Damage windows have no name typed in.
	if (DamageNotifyName.IsNone() || NotifyName == DamageNotifyName)
	{
		BeginMeleeTrace(TArray<FName>(), 0.f); // sockets + radius come from the attack asset
	}
}

void UVTGCombatComponent::HandleMontageNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	DebugLog(FString::Printf(TEXT("notify END   '%s'"), *NotifyName.ToString()));

	if (!ComboNotifyName.IsNone() && NotifyName == ComboNotifyName)
	{
		CloseComboWindow();
		return;
	}

	if (DamageNotifyName.IsNone() || NotifyName == DamageNotifyName)
	{
		EndMeleeTrace();
	}
}

// =================================================================================================
//  State
// =================================================================================================

bool UVTGCombatComponent::CanDoAction(EVTGCombatAction Action) const
{
	// One table, one answer. Adding a new state means editing this switch - not hunting down every
	// Branch chain in BP_Player_Sa.
	switch (CombatState)
	{
	case EVTGCombatState::Dead:
		return false;

	case EVTGCombatState::Cinematic:
		// Sequencer/dialogue owns the actor. Nothing gets through, including the timers that used
		// to keep firing under the old bool ("IsInCineCutscene_LockInput(NotWorkForMont)").
		return false;

	case EVTGCombatState::HitStun:
	case EVTGCombatState::Grabbed:
		return false;

	case EVTGCombatState::Downed:
		// Only getting up. Wake-up attacks would be added here later.
		return false;

	case EVTGCombatState::Attacking:
		// Mid-attack: only the combo window lets anything through.
		if (!bComboWindowOpen)
		{
			return false;
		}
		switch (Action)
		{
		case EVTGCombatAction::Attack:
		case EVTGCombatAction::HeavyAttack:
			return true;
		case EVTGCombatAction::Dodge:
		case EVTGCombatAction::Block:
			return CurrentAttack && CurrentAttack->bCanCancelInto;
		default:
			return false;
		}

	case EVTGCombatState::Dodging:
		return false;

	case EVTGCombatState::Blocking:
		// Can stop blocking into anything except shooting/aiming.
		return Action != EVTGCombatAction::Fire && Action != EVTGCombatAction::Aim;

	case EVTGCombatState::Grabbing:
		// Holding someone: throw/hit them, or let go.
		return Action == EVTGCombatAction::Attack
			|| Action == EVTGCombatAction::HeavyAttack
			|| Action == EVTGCombatAction::Grab
			|| Action == EVTGCombatAction::HeatAction;

	case EVTGCombatState::Idle:
	default:
		return true;
	}
}

void UVTGCombatComponent::SetCombatState(EVTGCombatState NewState)
{
	if (CombatState == NewState)
	{
		return;
	}

	// Dead is absorbing - nothing resurrects an actor by accident.
	if (CombatState == EVTGCombatState::Dead)
	{
		return;
	}

	const EVTGCombatState OldState = CombatState;
	CombatState = NewState;

	// Leaving Attacking for any reason must clean up the montage/movement/hitbox.
	if (OldState == EVTGCombatState::Attacking && NewState != EVTGCombatState::Attacking && !bChainingCombo)
	{
		CancelAttack();
	}

	OnCombatStateChanged.Broadcast(OldState, NewState);
}

// =================================================================================================
//  Attacking
// =================================================================================================

bool UVTGCombatComponent::TryAction(EVTGCombatAction Action)
{
	if (Action != EVTGCombatAction::Attack && Action != EVTGCombatAction::HeavyAttack)
	{
		// Non-attack verbs (dodge, block, grab...) aren't implemented yet - the gate answer is
		// still useful to Blueprint, so report it and let BP drive the rest for now.
		return CanDoAction(Action);
	}

	if (CanDoAction(Action))
	{
		UVTGAttackDataAsset* Next = (CombatState == EVTGCombatState::Attacking)
			? ResolveNextAttack(Action)
			: ResolveEntryAttack(Action);

		if (Next)
		{
			return StartAttack(Next);
		}
		// Combo has no follow-up for this button: fall through and buffer, so the press replays as
		// a fresh combo start the moment the current attack finishes.
	}

	// Too early - remember it. This is what makes a 0.05s-early press land instead of vanish into
	// the old `Delay 0.3`.
	if (CombatState == EVTGCombatState::Attacking || CombatState == EVTGCombatState::HitStun)
	{
		BufferedAction = Action;
		bHasBufferedAction = true;
		BufferedActionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	}

	return false;
}

bool UVTGCombatComponent::StartAttack(UVTGAttackDataAsset* Attack)
{
	if (!Attack || !IsAlive())
	{
		return false;
	}

	UAnimInstance* Anim = GetOwnerAnimInstance();
	if (!Anim || !Attack->Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGCombat] %s: attack '%s' has no montage or no anim instance."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Attack));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// The anim instance often doesn't exist yet at BeginPlay (mesh/AnimBP init order), so make sure
	// we're subscribed to its notify delegates before the first montage plays.
	BindMontageNotifies();

	// Chaining out of our own montage: suppress the teardown that the end-delegate would otherwise
	// run when Montage_Play interrupts the current one.
	bChainingCombo = (CombatState == EVTGCombatState::Attacking);

	CloseComboWindow();
	EndMeleeTrace();
	World->GetTimerManager().ClearTimer(ComboWindowOpenTimer);
	World->GetTimerManager().ClearTimer(ComboWindowCloseTimer);

	CurrentAttack = Attack;
	bHasBufferedAction = false;

	if (bFaceTargetOnAttack)
	{
		FaceBestTarget();
	}

	SetCombatState(EVTGCombatState::Attacking);
	SetMovementLocked(Attack->bLockMovement);

	const float Rate = FMath::Max(0.01f, Attack->PlayRate);
	Anim->Montage_Play(Attack->Montage, Rate);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UVTGCombatComponent::OnAttackMontageEnded);
	Anim->Montage_SetEndDelegate(EndDelegate, Attack->Montage);

	// Watchdog: if the montage's end callback never arrives, force the cleanup anyway shortly after
	// the montage should have finished. Being locked mid-attack forever is the worst failure this
	// component can have - it eats movement AND all input.
	const float ExpectedLength = Attack->Montage->GetPlayLength() / Rate;
	World->GetTimerManager().SetTimer(AttackWatchdogTimer, this,
		&UVTGCombatComponent::AttackWatchdog, ExpectedLength + 0.5f, false);

	DebugLog(FString::Printf(TEXT("StartAttack '%s' montage=%s len=%.2fs"),
		*GetNameSafe(Attack), *GetNameSafe(Attack->Montage), ExpectedLength));

	// Fallback combo window so combos work before ANS_ComboWindow is authored on the montages.
	// Once the notify state exists, tick bUseTimedComboWindow off on that attack asset.
	if (Attack->bUseTimedComboWindow && Attack->ComboWindowEnd > Attack->ComboWindowStart)
	{
		World->GetTimerManager().SetTimer(ComboWindowOpenTimer, this,
			&UVTGCombatComponent::OpenComboWindow, FMath::Max(0.01f, Attack->ComboWindowStart / Rate), false);
		World->GetTimerManager().SetTimer(ComboWindowCloseTimer, this,
			&UVTGCombatComponent::CloseComboWindow, FMath::Max(0.02f, Attack->ComboWindowEnd / Rate), false);
	}

	bChainingCombo = false;
	return true;
}

void UVTGCombatComponent::CancelAttack()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ComboWindowOpenTimer);
		World->GetTimerManager().ClearTimer(ComboWindowCloseTimer);
		World->GetTimerManager().ClearTimer(AttackWatchdogTimer);
	}

	if (CurrentAttack && CurrentAttack->Montage)
	{
		if (UAnimInstance* Anim = GetOwnerAnimInstance())
		{
			if (Anim->Montage_IsPlaying(CurrentAttack->Montage))
			{
				Anim->Montage_Stop(0.15f, CurrentAttack->Montage);
			}
		}
	}

	EndMeleeTrace();
	CloseComboWindow();
	SetMovementLocked(false);
	CurrentAttack = nullptr;
}

void UVTGCombatComponent::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// We interrupted ourselves to chain into the next move - that call already set up the new
	// state, so leave it alone.
	if (bChainingCombo)
	{
		return;
	}
	if (CurrentAttack && CurrentAttack->Montage != Montage)
	{
		return;
	}

	// Interrupted by ANYTHING else (a hit reaction, a cutscene, some other system playing a montage
	// into the same slot) still means the attack is over. Returning early here is what left the
	// character stuck in MOVE_None with the hitbox still open.
	DebugLog(FString::Printf(TEXT("montage ended (interrupted=%s)"), bInterrupted ? TEXT("yes") : TEXT("no")));
	FinishAttack();
}

void UVTGCombatComponent::AttackWatchdog()
{
	// Last line of defence. Montage end callbacks can go missing (montage stopped by another
	// system, slot not evaluated, montage looping), and when they do the old code left the actor
	// permanently locked mid-attack with the hitbox open - exactly the "stuck with a giant red
	// trace ball" bug. Never let that be permanent.
	if (CombatState == EVTGCombatState::Attacking)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VTGCombat] %s: attack '%s' never reported an end - forcing cleanup. Check the montage's slot and its notify window."),
			*GetNameSafe(GetOwner()), *GetNameSafe(CurrentAttack));
		FinishAttack();
	}
	else if (bTraceActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGCombat] %s: hitbox was still open after the attack ended - closing it."),
			*GetNameSafe(GetOwner()));
		EndMeleeTrace();
	}
}

void UVTGCombatComponent::FinishAttack()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackWatchdogTimer);
	}

	EndMeleeTrace();
	CloseComboWindow();
	SetMovementLocked(false);
	CurrentAttack = nullptr;

	if (CombatState == EVTGCombatState::Attacking)
	{
		SetCombatState(EVTGCombatState::Idle);
	}

	// A press that arrived too late for the combo window still starts a fresh combo, so mashing
	// through the end of a string feels continuous.
	TryConsumeBufferedAction();
}

void UVTGCombatComponent::OpenComboWindow()
{
	bComboWindowOpen = true;
	TryConsumeBufferedAction();
}

void UVTGCombatComponent::CloseComboWindow()
{
	bComboWindowOpen = false;
}

bool UVTGCombatComponent::TryConsumeBufferedAction()
{
	if (!bHasBufferedAction)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - BufferedActionTime > InputBufferWindow)
	{
		bHasBufferedAction = false;
		return false;
	}

	const EVTGCombatAction Action = BufferedAction;
	bHasBufferedAction = false;
	return TryAction(Action);
}

UVTGAttackDataAsset* UVTGCombatComponent::ResolveNextAttack(EVTGCombatAction Action) const
{
	if (!CurrentAttack)
	{
		return nullptr;
	}
	for (const TObjectPtr<UVTGAttackDataAsset>& Next : CurrentAttack->NextAttacks)
	{
		if (Next && Next->ContinueAction == Action)
		{
			return Next;
		}
	}
	// Branching combos list follow-ups in priority order; if none declared this button, take the
	// first entry so a plain 1-2-3 string only needs NextAttacks filled in.
	if (Action == EVTGCombatAction::Attack && CurrentAttack->NextAttacks.Num() > 0)
	{
		return CurrentAttack->NextAttacks[0];
	}
	return nullptr;
}

UVTGAttackDataAsset* UVTGCombatComponent::ResolveEntryAttack(EVTGCombatAction Action) const
{
	if (Action == EVTGCombatAction::HeavyAttack && DefaultHeavyAttack)
	{
		return DefaultHeavyAttack;
	}
	return DefaultAttack;
}

// =================================================================================================
//  Melee trace
// =================================================================================================

void UVTGCombatComponent::BeginMeleeTrace(const TArray<FName>& Sockets, float Radius)
{
	ActiveTraceSockets = Sockets;
	ActiveTraceRadius = Radius;

	// Fall back to the attack asset so the notify state can be authored with no parameters at all.
	if (ActiveTraceSockets.Num() == 0 && CurrentAttack)
	{
		ActiveTraceSockets = CurrentAttack->TraceSockets;
	}
	if (ActiveTraceRadius <= 0.f)
	{
		ActiveTraceRadius = CurrentAttack ? CurrentAttack->TraceRadius : 40.f;
	}

	HitActorsThisSwing.Reset();
	PreviousSocketLocations.Reset();
	bTraceActive = ActiveTraceSockets.Num() > 0;

	if (!bTraceActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGCombat] %s: hitbox opened but no trace sockets - fill TraceSockets on attack asset '%s'."),
			*GetNameSafe(GetOwner()), *GetNameSafe(CurrentAttack));
		return;
	}

	// Seed the previous positions so the first tick sweeps from where the hand actually was.
	if (const USkeletalMeshComponent* Mesh = GetOwnerMesh())
	{
		for (const FName& Socket : ActiveTraceSockets)
		{
			// A missing socket doesn't fail loudly in the engine - GetSocketLocation just falls back
			// to the component's own transform, so the hitbox silently sweeps around the character's
			// feet and hits nothing. Say so instead.
			if (!Mesh->DoesSocketExist(Socket))
			{
				ReportCombatProblem(FString::Printf(
					TEXT("attack '%s' uses socket '%s', which does not exist on %s. Check the socket name on the skeleton."),
					*GetNameSafe(CurrentAttack), *Socket.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset())));
			}
			PreviousSocketLocations.Add(Socket, Mesh->GetSocketLocation(Socket));
		}
	}

	SetComponentTickEnabled(true);
	DebugLog(FString::Printf(TEXT("hitbox OPEN  sockets=%d radius=%.0f"), ActiveTraceSockets.Num(), ActiveTraceRadius));
}

void UVTGCombatComponent::TickMeleeTrace()
{
	if (!bTraceActive)
	{
		return;
	}

	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	UWorld* World = GetWorld();
	if (!Mesh || !World)
	{
		return;
	}

	AActor* Owner = GetOwner();
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Owner);

	for (const FName& Socket : ActiveTraceSockets)
	{
		const FVector Current = Mesh->GetSocketLocation(Socket);
		const FVector* PrevPtr = PreviousSocketLocations.Find(Socket);
		const FVector Previous = PrevPtr ? *PrevPtr : Current;
		PreviousSocketLocations.Add(Socket, Current);

		// Sweep from last frame's position to this one: a fast fist can't skip past a target
		// between frames the way the old single-frame SphereTraceSingle did.
		TArray<FHitResult> Hits;
		UKismetSystemLibrary::SphereTraceMultiForObjects(
			this, Previous, Current, ActiveTraceRadius, TraceObjectTypes,
			/*bTraceComplex=*/false, ActorsToIgnore, TraceDebugDraw, Hits, /*bIgnoreSelf=*/true);

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActorsThisSwing.Contains(HitActor))
			{
				continue; // one hit per actor per swing - no double-tapping on a single punch
			}

			// Consume the actor whether or not the hit lands. Rejected targets (walls, teammates,
			// corpses) then get evaluated - and logged - exactly once per swing instead of every tick.
			HitActorsThisSwing.Add(HitActor);
			ApplyHitTo(HitActor, Hit);
		}
	}
}

void UVTGCombatComponent::EndMeleeTrace()
{
	if (bTraceActive)
	{
		// "touched 0" means the sweep never overlapped anything: wrong socket, radius too small,
		// or the target's collision isn't in TraceObjectTypes. That's a different bug from
		// "touched the enemy but he ignored it", so say which one happened.
		DebugLog(FString::Printf(TEXT("hitbox CLOSED - touched %d actor(s) this swing"), HitActorsThisSwing.Num()));
	}
	bTraceActive = false;
	ActiveTraceSockets.Reset();
	PreviousSocketLocations.Reset();
	HitActorsThisSwing.Reset();
	SetComponentTickEnabled(false);
}

bool UVTGCombatComponent::ApplyHitTo(AActor* Target, const FHitResult& HitResult)
{
	AActor* Owner = GetOwner();
	if (!Target || !Owner || Target == Owner)
	{
		return false;
	}
	// Every rejection below used to be silent, which made "I punched him and nothing happened"
	// impossible to diagnose. Each one now says exactly why.
	if (!Target->GetClass()->ImplementsInterface(UVTGDamageable::StaticClass()))
	{
		ReportCombatProblem(FString::Printf(
			TEXT("swung at '%s' but it does not implement VTGDamageable - add the interface in its Class Settings."),
			*GetNameSafe(Target)));
		return false;
	}
	if (!IsValidTarget(Target))
	{
		DebugLog(FString::Printf(TEXT("swung at '%s' - rejected (dead, or same TeamId as me: %d)"),
			*GetNameSafe(Target), TeamId));
		return false;
	}

	FVTGHitEvent Event;
	Event.Instigator = Owner;
	if (CurrentAttack)
	{
		Event.AttackId = CurrentAttack->AttackId;
		Event.Damage = CurrentAttack->Damage;
		Event.ReactType = CurrentAttack->HitReactType;
		Event.Launch = CurrentAttack->LaunchImpulse;
		Event.HitStop = CurrentAttack->HitStopDuration;
		Event.HitStunDuration = CurrentAttack->HitStunDuration;
		Event.bBlockable = CurrentAttack->bBlockable;
	}
	Event.HitLocation = HitResult.ImpactPoint.IsZero() ? Target->GetActorLocation() : HitResult.ImpactPoint;
	Event.HitNormal = HitResult.ImpactNormal;
	Event.HitBone = HitResult.BoneName;

	// Snapshot the victim's health so we can tell whether the hit actually did anything. A Blueprint
	// that adds the interface but never implements the event swallows Execute_ReceiveCombatHit
	// silently - no error, no damage, nothing. That's the single most common wiring mistake here,
	// so detect it explicitly instead of letting it look like "combat is broken".
	UVTGCombatComponent* TargetCombat = Target->FindComponentByClass<UVTGCombatComponent>();
	const float HealthBefore = TargetCombat ? TargetCombat->GetCurrentHealth() : -1.f;

	IVTGDamageable::Execute_ReceiveCombatHit(Target, Event);

	if (TargetCombat && FMath::IsNearlyEqual(HealthBefore, TargetCombat->GetCurrentHealth()))
	{
		ReportCombatProblem(FString::Printf(
			TEXT("hit '%s' but its health did not change. Its 'Receive Combat Hit' event is missing or not wired to 'Handle Incoming Hit'."),
			*GetNameSafe(Target)));
	}
	else
	{
		DebugLog(FString::Printf(TEXT("HIT '%s' for %.0f (health %.0f -> %.0f)"),
			*GetNameSafe(Target), Event.Damage, HealthBefore,
			TargetCombat ? TargetCombat->GetCurrentHealth() : -1.f));
	}

	// One number, both actors, one place. This is the whole reason hitstop used to feel wrong:
	// the player froze for 0.1s and the enemy for 0.15s, decided in two unrelated graphs.
	if (Event.HitStop > 0.f)
	{
		ApplyHitStop(Owner, Event.HitStop);
		ApplyHitStop(Target, Event.HitStop);
	}

	OnHitLanded.Broadcast(Target, Event);
	return true;
}

// =================================================================================================
//  Taking damage
// =================================================================================================

void UVTGCombatComponent::HandleIncomingHit(const FVTGHitEvent& Hit)
{
	if (!IsAlive())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVTGHitEvent Applied = Hit;

	// Block check: only counts if we're guarding, the move is blockable, and it came from the front.
	if (CombatState == EVTGCombatState::Blocking && Hit.bBlockable && Hit.Instigator)
	{
		const FVector ToAttacker = (Hit.Instigator->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(), ToAttacker);
		if (Dot >= FMath::Cos(FMath::DegreesToRadians(BlockHalfAngle)))
		{
			Applied.Damage *= BlockDamageMultiplier;
			Applied.ReactType = EVTGHitReact::Guard;
			Applied.Launch *= 0.3f;
			Applied.HitStunDuration *= 0.5f;
		}
	}

	CurrentHealth = FMath::Max(0.f, CurrentHealth - Applied.Damage);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnHitReceived.Broadcast(Applied);

	DebugLog(FString::Printf(TEXT("TOOK %.0f from '%s' (react=%d) -> health %.0f/%.0f"),
		Applied.Damage, *GetNameSafe(Applied.Instigator), (int32)Applied.ReactType, CurrentHealth, MaxHealth));

	if (CurrentHealth <= 0.f)
	{
		HandleDeath(Applied.Instigator);
		return;
	}

	// A landed hit beats whatever we were doing (unless it was guarded).
	if (Applied.ReactType != EVTGHitReact::None && Applied.ReactType != EVTGHitReact::Guard)
	{
		if (CombatState == EVTGCombatState::Attacking)
		{
			CancelAttack();
		}
		EnterHitStun(Applied.HitStunDuration);
	}

	PlayHitReact(Applied);

	// Knockback, in attacker space by default so one asset works at any fight orientation.
	if (!Applied.Launch.IsNearlyZero())
	{
		if (ACharacter* OwnerChar = Cast<ACharacter>(Owner))
		{
			FVector WorldLaunch = Applied.Launch;
			if (Applied.bLaunchIsAttackerRelative && Applied.Instigator)
			{
				const FRotator AttackerYaw(0.f, Applied.Instigator->GetActorRotation().Yaw, 0.f);
				WorldLaunch = AttackerYaw.RotateVector(Applied.Launch);
			}
			OwnerChar->LaunchCharacter(WorldLaunch, /*bXYOverride=*/true, /*bZOverride=*/WorldLaunch.Z > 0.f);
		}
	}
}

void UVTGCombatComponent::HandleLegacyDamage(float Damage, AActor* Causer)
{
	FVTGHitEvent Event;
	Event.Damage = Damage;
	Event.Instigator = Causer;
	Event.ReactType = EVTGHitReact::Light;
	Event.HitStop = 0.f; // legacy senders already ran their own hitstop; don't double it
	Event.HitLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	HandleIncomingHit(Event);
}

void UVTGCombatComponent::Heal(float Amount)
{
	if (!IsAlive() || Amount <= 0.f)
	{
		return;
	}
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UVTGCombatComponent::Kill(AActor* Killer)
{
	if (!IsAlive())
	{
		return;
	}
	CurrentHealth = 0.f;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	HandleDeath(Killer);
}

void UVTGCombatComponent::HandleDeath(AActor* Killer)
{
	CancelAttack();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStunTimer);
	}

	// Set the state directly: SetCombatState refuses to leave Dead, and CancelAttack already ran.
	const EVTGCombatState OldState = CombatState;
	CombatState = EVTGCombatState::Dead;
	OnCombatStateChanged.Broadcast(OldState, CombatState);

	// Ragdoll, loot, the BPI_OnEnmDeath -> BPLM_L2StreetBarFight cutscene hook: all still Blueprint,
	// now hanging off one event instead of being buried in the damage graph.
	OnDeath.Broadcast(Killer);
}

void UVTGCombatComponent::PlayHitReact(const FVTGHitEvent& Hit)
{
	if (Hit.ReactType == EVTGHitReact::None)
	{
		return;
	}

	UAnimInstance* Anim = GetOwnerAnimInstance();
	if (!Anim)
	{
		return;
	}

	TObjectPtr<UAnimMontage> const* Found = HitReactMontages.Find(Hit.ReactType);
	if (!Found || !*Found)
	{
		// Missing tier falls back to Light so a half-authored reaction set still plays something.
		Found = HitReactMontages.Find(EVTGHitReact::Light);
	}
	if (Found && *Found)
	{
		Anim->Montage_Play(*Found);
	}
}

void UVTGCombatComponent::EnterHitStun(float Duration)
{
	SetCombatState(EVTGCombatState::HitStun);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(HitStunTimer);
	World->GetTimerManager().SetTimer(HitStunTimer, this, &UVTGCombatComponent::ExitHitStun,
		FMath::Max(0.02f, Duration), false);
}

void UVTGCombatComponent::ExitHitStun()
{
	if (CombatState == EVTGCombatState::HitStun)
	{
		SetCombatState(EVTGCombatState::Idle);
		TryConsumeBufferedAction();
	}
}

void UVTGCombatComponent::ApplyHitStop(AActor* Actor, float Duration, float TimeDilation)
{
	if (!IsValid(Actor) || Duration <= 0.f)
	{
		return;
	}
	UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return;
	}

	Actor->CustomTimeDilation = FMath::Clamp(TimeDilation, 0.f, 1.f);

	// Timers run on world time, not the actor's CustomTimeDilation, so the freeze lasts the real
	// duration asked for - and both actors unfreeze on the same frame.
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Actor, [Actor]()
	{
		Actor->CustomTimeDilation = 1.f;
	}), Duration, false);
}

// =================================================================================================
//  Helpers
// =================================================================================================

void UVTGCombatComponent::ReportCombatProblem(const FString& Message) const
{
	// Wiring mistakes are always reported, regardless of bDebugLog, and put on screen too - the
	// whole class of "nothing happens and nothing tells you why" bugs is what this exists for.
	UE_LOG(LogTemp, Warning, TEXT("[VTGCombat] %s: %s"), *GetNameSafe(GetOwner()), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key=*/5551, 6.f, FColor::Orange,
			FString::Printf(TEXT("[VTGCombat] %s: %s"), *GetNameSafe(GetOwner()), *Message));
	}
}

void UVTGCombatComponent::DebugLog(const FString& Message) const
{
	if (!bDebugLog)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[VTGCombat] %s: %s"), *GetNameSafe(GetOwner()), *Message);
}

bool UVTGCombatComponent::IsValidTarget(AActor* Target) const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Target) || Target == Owner)
	{
		return false;
	}

	// Ask the target's combat component, not an interface function: an interface function with a
	// return value gets an auto-created EMPTY graph in every Blueprint that adds the interface, and
	// an empty graph returns false - which would make the actor silently unhittable.
	const UVTGCombatComponent* TargetCombat = Target->FindComponentByClass<UVTGCombatComponent>();
	if (!TargetCombat)
	{
		// No combat component (a breakable prop): it handles ReceiveCombatHit itself. Let it through.
		return true;
	}

	if (!TargetCombat->IsAlive())
	{
		return false; // don't waste combo hits on corpses
	}

	// Same team, and the team is a real one: no friendly fire.
	if (TeamId >= 0 && TargetCombat->TeamId == TeamId)
	{
		return false;
	}

	return true;
}

UAnimInstance* UVTGCombatComponent::GetOwnerAnimInstance() const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

USkeletalMeshComponent* UVTGCombatComponent::GetOwnerMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (const ACharacter* OwnerChar = Cast<ACharacter>(Owner))
	{
		return OwnerChar->GetMesh();
	}
	return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

void UVTGCombatComponent::SetMovementLocked(bool bLocked)
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar)
	{
		return;
	}
	UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (bLocked)
	{
		if (Movement->MovementMode != MOVE_None)
		{
			Movement->SetMovementMode(MOVE_None);
		}
	}
	else if (Movement->MovementMode == MOVE_None)
	{
		// Only restore if we're not airborne - falling out of an air combo must stay falling.
		Movement->SetMovementMode(MOVE_Walking);
	}
}

void UVTGCombatComponent::FaceBestTarget()
{
	AActor* Owner = GetOwner();
	AActor* Target = FindSoftLockTarget();
	if (!Owner || !Target)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	FRotator NewRotation = Owner->GetActorRotation();
	NewRotation.Yaw = ToTarget.Rotation().Yaw;
	Owner->SetActorRotation(NewRotation);
}

AActor* UVTGCombatComponent::FindSoftLockTarget() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<AActor*> Overlapping;
	TArray<AActor*> ToIgnore;
	ToIgnore.Add(Owner);
	UKismetSystemLibrary::SphereOverlapActors(this,
		Owner->GetActorLocation(), SoftLockRadius, TraceObjectTypes, nullptr, ToIgnore, Overlapping);

	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(SoftLockHalfAngle));

	AActor* Best = nullptr;
	float BestScore = -1.f;

	for (AActor* Candidate : Overlapping)
	{
		if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UVTGDamageable::StaticClass()))
		{
			continue;
		}
		if (!IsValidTarget(Candidate))
		{
			continue;
		}

		const FVector ToCandidate = (Candidate->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(Forward, ToCandidate);
		if (Dot < MinDot)
		{
			continue; // outside the cone in front of us
		}

		// Most-in-front wins, distance breaks near-ties - snapping to whoever you're facing reads
		// better than snapping to whoever is closest.
		const float Distance = FVector::Dist2D(Candidate->GetActorLocation(), Owner->GetActorLocation());
		const float Score = Dot - (Distance / FMath::Max(1.f, SoftLockRadius)) * 0.25f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}
