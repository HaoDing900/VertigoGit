#include "VTGLevelManagerBase.h"
#include "VTGGameInstanceBase.h"
#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Combat/VTGCombatPresentation.h"
#include "VTGMailReadingComponent.h"
#include "NarrativeComponent.h"

bool AVTGLevelManagerBase::DeferEncounterUntilDialogueEnds(UNarrativeComponent* Narrative,FName ResumeEvent)
{
 if(bDispatchingEncounterResume || !Narrative || !Narrative->GetCurrentDialogue())return false;
 auto* Event=FindFunction(ResumeEvent);
 if(!Event || Event->NumParms!=0){UE_LOG(LogTemp,Error,TEXT("Invalid deferred encounter event %s"),*ResumeEvent.ToString());return true;}
 if(auto* Previous=DeferredEncounterNarrative.Get())Previous->OnDialogueFinished.RemoveDynamic(this,&AVTGLevelManagerBase::HandleDeferredEncounterDialogueFinished);
 DeferredEncounterNarrative=Narrative;DeferredEncounterEvent=ResumeEvent;
 Narrative->OnDialogueFinished.AddUniqueDynamic(this,&AVTGLevelManagerBase::HandleDeferredEncounterDialogueFinished);
 return true;
}
void AVTGLevelManagerBase::HandleDeferredEncounterDialogueFinished(UDialogue*,bool)
{
 if(bDeferredEncounterQueued || !GetWorld())return;
 bDeferredEncounterQueued=true;
 // OnDialogueFinished fires before CurrentDialogue is cleared and before all
 // Blueprint input/camera restoration callbacks have run. Resume next tick.
 GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){ResumeDeferredEncounterWhenIdle();}));
}
void AVTGLevelManagerBase::ResumeDeferredEncounterWhenIdle()
{
 bDeferredEncounterQueued=false;
 auto* Narrative=DeferredEncounterNarrative.Get();
 if(!Narrative || Narrative->GetCurrentDialogue())return; // A replacement dialogue still owns the scene.
 Narrative->OnDialogueFinished.RemoveDynamic(this,&AVTGLevelManagerBase::HandleDeferredEncounterDialogueFinished);
 const FName Event=DeferredEncounterEvent;DeferredEncounterEvent=NAME_None;DeferredEncounterNarrative.Reset();
 if(auto* Function=FindFunction(Event))ProcessEvent(Function,nullptr);
}

void AVTGLevelManagerBase::BeginStoryNoteMailReading(UObject* NoteAsset)
{
 if (auto* Mail = UVTGMailReadingComponent::GetOrCreate(UGameplayStatics::GetPlayerPawn(this, 0)))
  Mail->SetHold(this, bKeepMailReadingDuringStoryNote && IsValid(NoteAsset));
}

void AVTGLevelManagerBase::EndStoryNoteMailReading()
{
 if (auto* Mail = UVTGMailReadingComponent::Find(UGameplayStatics::GetPlayerPawn(this, 0)))
  Mail->SetHold(this, false);
}

bool AVTGLevelManagerBase::SetPlayerStoryCombatLock(bool Enabled)
{
 if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))
  if(auto* Presentation=Player->FindComponentByClass<UVTGPlayerPresentationComponent>())
  {Presentation->SetStoryCombatLock(Enabled);return true;}
 return false;
}

UUserWidget* AVTGLevelManagerBase::FadeConfigured(double TimeBeforeKill,double HoldDurationUntilFade,double FadeDuration,bool DisableFade,bool ReverseFadeOnAppear,int32 ZOrder)
{
 auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/2DArt/ScreenTransition/WBP_Fade.WBP_Fade_C"));
 auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!Class || !PC)return nullptr;
 auto* Fade=CreateWidget<UUserWidget>(PC,Class);if(!Fade)return nullptr;
 for(const auto& Pair:{TPair<FName,double>(TEXT("TimeBeforeKill"),TimeBeforeKill),TPair<FName,double>(TEXT("HoldDurationUntilFade"),FMath::Max(0.,HoldDurationUntilFade)),TPair<FName,double>(TEXT("FadeDuration"),FMath::Max(0.,FadeDuration))})
  if(auto* P=FindFProperty<FNumericProperty>(Class,Pair.Key))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(Fade),Pair.Value);
 for(const auto& Pair:{TPair<FName,bool>(TEXT("DisableFade?"),DisableFade),TPair<FName,bool>(TEXT("ReverseFadeOnAppear"),ReverseFadeOnAppear)})
  if(auto* P=FindFProperty<FBoolProperty>(Class,Pair.Key))P->SetPropertyValue_InContainer(Fade,Pair.Value);
 Fade->AddToViewport(ZOrder);return Fade;
}

AVTGLevelManagerBase::AVTGLevelManagerBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

FName AVTGLevelManagerBase::GetActiveStage() const
{
	const UWorld* StageWorld = GetWorld();
	const UVTGGameInstanceBase* StageGI = StageWorld ? StageWorld->GetGameInstance<UVTGGameInstanceBase>() : nullptr;
	// Only a direct PIE launch may use the designer override. Explicit travel
	// and save loading keep their stage for the remainder of that game session.
	if (bOverrideStageForTesting && StageWorld && StageWorld->WorldType == EWorldType::PIE
		&& (!StageGI || !StageGI->HasExplicitStage()))
	{
		return TestStage;
	}

	//Otherwise read the value the previous map stashed on the (persistent) game instance.
	if (const UWorld* World = GetWorld())
	{
		if (const UVTGGameInstanceBase* GI = World->GetGameInstance<UVTGGameInstanceBase>())
		{
			return GI->CurrentStage;
		}
	}

	return NAME_None;
}

FName AVTGLevelManagerBase::GetResumeCheckpoint() const
{
	const UGameInstance* GI = GetGameInstance();
	const UVTGSaveCoordinator* Save = GI ? GI->GetSubsystem<UVTGSaveCoordinator>() : nullptr;
	return Save ? Save->GetResumeCheckpoint() : NAME_None;
}

void AVTGLevelManagerBase::BeginPlay()
{
 // Restore before Blueprint ReceiveBeginPlay and OnStageBegin can start narrative.
 bool bRestoreInFlight = false;
 if (UGameInstance* GI = GetGameInstance())
  if (auto* Save = GI->GetSubsystem<UVTGSaveCoordinator>())
  {
   Save->RestoreLevelManager(this);
   bRestoreInFlight = Save->HasPendingWorldRestore();
   // The hard initiate waits for the same broadcast the encounter resume does, so setup always lands
   // before the resume rebuilds the scene on top of it.
   if (bRestoreInFlight || bEncounterResumePending)
    Save->OnSlotLoaded.AddUniqueDynamic(this, &AVTGLevelManagerBase::HandleEncounterSlotLoaded);
  }
 // Armed on every path: a normal arrival fires from here next tick, and a restore that ends without
 // ever broadcasting still gets its setup instead of silently losing it.
 GetWorldTimerManager().SetTimerForNextTick(this, &AVTGLevelManagerBase::PollLevelReadyAfterRestore);
 // One line that separates the three ways level setup can fail to happen: the stage being empty, the
 // stage event being skipped, and the hard initiate never being named. Filter the log on "VTG LEVEL".
 UE_LOG(LogTemp, Display, TEXT("VTG LEVEL %s BeginPlay: activeStage=%s subStage=%s restoreInFlight=%d encounterResume=%d (OnStageBegin %s) hardInitiateEvent=%s"),
  *GetName(), *GetActiveStage().ToString(), SubStage.IsNone() ? TEXT("(none)") : *SubStage.ToString(),
  bRestoreInFlight, bEncounterResumePending,
  bEncounterResumePending ? TEXT("SKIPPED") : TEXT("runs"),
  HardInitiateEvent.IsNone() ? TEXT("(none set)") : *HardInitiateEvent.ToString());
 Super::BeginPlay();

	//Hand the resolved stage to the Blueprint child so it can run its Chain-A / Chain-B logic.
 if (!bEncounterResumePending)
 {
  const FName Stage = GetActiveStage();
  OnStageBegin(Stage);
  // Entering a new stage is the story beat we autosave on. The coordinator decides whether this one
  // counts (same stage again, a load still in flight, no pawn yet) and defers the write a tick.
  if (UGameInstance* GI = GetGameInstance())
   if (auto* Save = GI->GetSubsystem<UVTGSaveCoordinator>()) Save->StoryAutoSave(GetWorld(), Stage);
 }
}

void AVTGLevelManagerBase::EndPlay(const EEndPlayReason::Type Reason)
{
 if(auto* Narrative=DeferredEncounterNarrative.Get())Narrative->OnDialogueFinished.RemoveDynamic(this,&AVTGLevelManagerBase::HandleDeferredEncounterDialogueFinished);
 DeferredEncounterEvent=NAME_None;DeferredEncounterNarrative.Reset();
 if (auto* GI = GetGameInstance())
  if (auto* Save = GI->GetSubsystem<UVTGSaveCoordinator>()) Save->OnSlotLoaded.RemoveDynamic(this, &AVTGLevelManagerBase::HandleEncounterSlotLoaded);
 Super::EndPlay(Reason);
}

bool AVTGLevelManagerBase::SetEncounterResumeEvent(FName ResumeEvent)
{
 UFunction* Event = FindFunction(ResumeEvent);
 if (!Event || Event->NumParms != 0)
 {
  UE_LOG(LogTemp, Error, TEXT("%s: checkpoint resume event %s must exist and have no parameters"), *GetName(), *ResumeEvent.ToString());
  return false;
 }
 EncounterResumeEvent = ResumeEvent;
 return true;
}

bool AVTGLevelManagerBase::SaveEncounterCheckpoint(FName CheckpointId, FName ResumeEvent, const FTransform& RespawnTransform, int32 Order)
{
 // Rebuilding a loaded encounter must not replace its original story snapshot or respawn point.
 if (bDispatchingEncounterResume) return true;
 if (CheckpointId.IsNone()) return false;
 auto* GI = GetGameInstance();
 auto* Save = GI ? GI->GetSubsystem<UVTGSaveCoordinator>() : nullptr;
 if (!Save || Save->HasPendingWorldRestore() || !UGameplayStatics::GetPlayerPawn(this, 0)) return false;
 const FName PreviousEvent = EncounterResumeEvent, PreviousId = EncounterCheckpointId;
 if (!SetEncounterResumeEvent(ResumeEvent)) return false;
 EncounterCheckpointId = CheckpointId;
 const bool Saved = Save->SaveCheckpoint(CheckpointId, RespawnTransform, Order);
 if (!Saved)
 {
  EncounterResumeEvent = PreviousEvent;
  EncounterCheckpointId = PreviousId;
  UE_LOG(LogTemp, Warning, TEXT("%s: failed to save encounter checkpoint %s"), *GetName(), *CheckpointId.ToString());
 }
 return Saved;
}

void AVTGLevelManagerBase::HandleEncounterSlotLoaded(int32 Slot)
{
 // Setup first: the resume event rebuilds the encounter on top of the level, not instead of it.
 FireLevelReady(/*bFromLoad=*/true);
 ResumeSavedEncounter();
}

void AVTGLevelManagerBase::PollLevelReadyAfterRestore()
{
 if (bLevelReadyFired) return;
 auto* GI = GetGameInstance();
 auto* Save = GI ? GI->GetSubsystem<UVTGSaveCoordinator>() : nullptr;
 // Still loading: PendingLoad is only cleared after OnSlotLoaded, so the delegate normally wins.
 if (Save && Save->HasPendingWorldRestore())
 {
  GetWorldTimerManager().SetTimerForNextTick(this, &AVTGLevelManagerBase::PollLevelReadyAfterRestore);
  return;
 }
 FireLevelReady(/*bFromLoad=*/bProgressRestored);
}

void AVTGLevelManagerBase::FireLevelReady(bool bFromLoad)
{
 if (bLevelReadyFired) return;
 bLevelReadyFired = true;
 // Readable from the event itself, so one-time beats can branch without the caller passing a pin.
 bEnteredFromLoad = bFromLoad;
 const bool bRan = RunHardInitiate();
 UE_LOG(LogTemp, Display, TEXT("VTG LEVEL %s hard initiate: event=%s fromLoad=%d activeStage=%s subStage=%s -> %s"),
  *GetName(), HardInitiateEvent.IsNone() ? TEXT("(none set)") : *HardInitiateEvent.ToString(), bFromLoad,
  *GetActiveStage().ToString(), SubStage.IsNone() ? TEXT("(none)") : *SubStage.ToString(),
  bRan ? TEXT("called") : TEXT("NOT CALLED"));
}

bool AVTGLevelManagerBase::RunHardInitiate()
{
 if (HardInitiateEvent.IsNone()) return false;
 UFunction* Event = FindFunction(HardInitiateEvent);
 if (!Event || Event->NumParms != 0)
 {
  // Named but unreachable is the silent failure this whole mechanism exists to avoid: a renamed or
  // mistyped event would simply never set the level up, exactly like the bug it replaces.
  UE_LOG(LogTemp, Error, TEXT("%s: hard initiate event \"%s\" must exist on this Blueprint and take no parameters - the level will not be set up"),
   *GetName(), *HardInitiateEvent.ToString());
  return false;
 }
 ProcessEvent(Event, nullptr);
 return true;
}

void AVTGLevelManagerBase::ResumeSavedEncounter()
{
 if (!bEncounterResumePending) return;
 bEncounterResumePending = false;
 if (UFunction* Event = FindFunction(EncounterResumeEvent))
 {
  if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
  {
   // Blueprint parent initialization can be delayed; rebuild caches before the scene entry uses them.
   auto Cache = [this](FName Name, UObject* Value)
   {
    if (auto* P = FindFProperty<FObjectPropertyBase>(GetClass(), Name))
     if (Value && Value->IsA(P->PropertyClass)) P->SetObjectPropertyValue_InContainer(this, Value);
   };
   Cache(TEXT("Player Controller"), PC);
   Cache(TEXT("GameInstance"), GetGameInstance());
   PC->ResetIgnoreInputFlags();
   if (auto* Pawn = PC->GetPawn())
   {
    Cache(TEXT("Player Pawn"), Pawn);
    if (auto* P = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("Narrative")))
     for (UActorComponent* Component : Pawn->GetComponents())
      if (Component && Component->IsA(P->PropertyClass)) { Cache(TEXT("Narrative"), Component); break; }
    Pawn->EnableInput(PC);
    PC->SetViewTarget(Pawn);
    PC->SetControlRotation(Pawn->GetActorRotation());
    if (auto* Character = Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
   }
  }
  TGuardValue<bool> Resuming(bDispatchingEncounterResume, true);
  ProcessEvent(Event, nullptr);
 }
}

namespace
{
 // Shared with the GameInstance snapshot so the two definitions cannot drift apart.
 bool IsProgressValue(const FProperty* P) { return UVTGSaveStatics::IsValueProperty(P); }
 bool IsProgressProperty(const FProperty* P)
 {
  if (P->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) return false;
  const UClass* Owner = P->GetOwnerClass();
  if (!Owner || !Owner->HasAnyClassFlags(CLASS_CompiledFromBlueprint)) return false;
  // Common BPLM camera/input/sequence bookkeeping must start clean after loading.
  if (Owner->GetName() == TEXT("BPLM_C") && P->GetFName() != TEXT("GeneralLevelPhase") && !P->HasAnyPropertyFlags(CPF_SaveGame)) return false;
  return IsProgressValue(P);
 }
}

bool AVTGLevelManagerBase::StoryCheckpoint(FName InSubStage)
{
 if (InSubStage.IsNone())
 {
  // Not an assert: a Blueprint pin left empty is a mistake worth seeing, not worth crashing over.
  UE_LOG(LogTemp, Warning, TEXT("VTG LEVEL %s story checkpoint called with no name - nothing recorded and nothing saved. The name is both the sub-stage the hard initiate rebuilds from and the autosave's beat, so it cannot be empty."), *GetName());
  return false;
 }
 SubStage = InSubStage;
 UGameInstance* GI = GetGameInstance();
 UVTGSaveCoordinator* Save = GI ? GI->GetSubsystem<UVTGSaveCoordinator>() : nullptr;
 // The sub-stage IS the beat. Keying the autosave on the map plus this name means two checkpoints in
 // one map are always two different beats, and re-reaching the same one does not write again.
 const bool bScheduled = Save && Save->StoryAutoSave(GetWorld(), InSubStage);
 UE_LOG(LogTemp, Display, TEXT("VTG LEVEL %s story checkpoint: subStage=%s stage=%s -> %s"),
  *GetName(), *InSubStage.ToString(), *GetActiveStage().ToString(),
  bScheduled ? TEXT("autosave scheduled") : TEXT("sub-stage recorded, no autosave (see the coordinator's own log line for why)"));
 return bScheduled;
}

void AVTGLevelManagerBase::SetManualSaveBlocked(bool bBlocked)
{
 if (bManualSaveBlocked == bBlocked) return;
 bManualSaveBlocked = bBlocked;
 // Logged on every change: "the save button is greyed out and nobody knows why" is the failure this
 // feature invites, and one grep for "VTG LEVEL" has to be enough to answer it.
 UE_LOG(LogTemp, Display, TEXT("VTG LEVEL %s manual saving %s (subStage=%s). The combat checkpoint is unaffected either way."),
  *GetName(), bBlocked ? TEXT("BLOCKED") : TEXT("allowed again"),
  SubStage.IsNone() ? TEXT("(none)") : *SubStage.ToString());
}

void AVTGLevelManagerBase::ClearSubStage()
{
 UE_LOG(LogTemp, Display, TEXT("VTG LEVEL %s sub-stage cleared (was %s)"), *GetName(), *SubStage.ToString());
 SubStage = NAME_None;
}

bool AVTGLevelManagerBase::EnterSavedStoryEvent(FName EventId)
{
 if (EventId.IsNone() || RestoredStoryEvents.Contains(EventId)) return false;
 StartedStoryEvents.Add(EventId);
 return true;
}

void AVTGLevelManagerBase::CaptureLevelProgress(FVTGLevelProgressRecord& Record) const
{
 Record.ClassPath = GetClass()->GetPathName();
 Record.StartedEvents = StartedStoryEvents;
 Record.EncounterResumeEvent = EncounterResumeEvent;
 Record.EncounterCheckpointId = EncounterCheckpointId;
 Record.SubStage = SubStage;
 Record.Values.Reset();
 Record.Types.Reset();
 for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
 {
  const FProperty* P = *It;
  if (!IsProgressProperty(P) || ExcludedProgressVariables.Contains(P->GetFName())) continue;
  FString Value;
  P->ExportText_InContainer(0, Value, this, this, const_cast<AVTGLevelManagerBase*>(this), PPF_None);
  Record.Values.Add(P->GetFName(), Value);
  Record.Types.Add(P->GetFName(), P->GetCPPType());
 }
}

void AVTGLevelManagerBase::RestoreLevelProgress(const FVTGLevelProgressRecord& Record)
{
 if (!Record.ClassPath.IsEmpty() && Record.ClassPath != GetClass()->GetPathName())
 {
  // The exact path used to be required, so moving or renaming a BPLM asset - or reparenting it - threw
  // away every existing save's progress for it without a word. A save written by an ancestor class is
  // still readable, because the variables it recorded all exist on this one. Anything else is a real
  // mismatch and is dropped, but loudly.
  const UClass* Saved = FindObject<UClass>(nullptr, *Record.ClassPath);
  if (!Saved || !GetClass()->IsChildOf(Saved))
  {
   UE_LOG(LogTemp, Warning, TEXT("%s: the save holds progress written by %s, which is neither this class nor an ancestor of it - that progress is being dropped"), *GetName(), *Record.ClassPath);
   return;
  }
 }
 for (const auto& Pair : Record.Values)
 {
  FProperty* P = FindFProperty<FProperty>(GetClass(), Pair.Key);
  const FString* Type = Record.Types.Find(Pair.Key);
  if (P && IsProgressProperty(P) && !ExcludedProgressVariables.Contains(P->GetFName()) && Type && *Type == P->GetCPPType())
   P->ImportText_Direct(*Pair.Value, P->ContainerPtrToValuePtr<void>(this), this, PPF_None);
 }
 bProgressRestored = true;
 StartedStoryEvents = Record.StartedEvents;
 RestoredStoryEvents = Record.StartedEvents;
 EncounterResumeEvent = Record.EncounterResumeEvent;
 EncounterCheckpointId = Record.EncounterCheckpointId;
 // Restored here, at the top of BeginPlay, which puts it in place well before the hard initiate reads
 // it. A save written before sub-stages existed simply has None, which is the stage's normal setup.
 SubStage = Record.SubStage;
 UFunction* Resume = FindFunction(EncounterResumeEvent);
 bEncounterResumePending = Resume && Resume->NumParms == 0;
}
