#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Save/VTGSaveTypes.h"
#include "VTGLevelManagerBase.generated.h"
class UUserWidget;

/**
 * Base class for level managers (reparent BPLM to this).
 *
 * Holds an in-editor test override so you can force a stage while editing, and resolves the
 * "active stage" purely from already-available data (test override, else the GameInstance value).
 * Because GetActiveStage is a pure lookup - not something computed in BeginPlay - any other actor
 * can ask for the stage in its own BeginPlay, in any order, and always get the right answer. The
 * manager does NOT need to run before everyone else.
 *
 * Stage is a plain Name, matching your existing structure.
 */
UCLASS()
class VERTIGO_API AVTGLevelManagerBase : public AActor
{
	GENERATED_BODY()

public:

	AVTGLevelManagerBase();
 /** Keep the terminal reading pose until a story Note opened from mail has closed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mail|Story Note")
 bool bKeepMailReadingDuringStoryNote = true;

 UFUNCTION(BlueprintCallable, Category="Mail|Story Note")
 void BeginStoryNoteMailReading(UObject* NoteAsset);

 UFUNCTION(BlueprintCallable, Category="Mail|Story Note")
 void EndStoryNoteMailReading();
 /** True: empty hands and hold out of combat. False: release the lock without auto-equipping. */
 UFUNCTION(BlueprintCallable, Category="Story|Combat", meta=(DisplayName="Set Player Story Combat Lock"))
 bool SetPlayerStoryCombatLock(bool Enabled);
 /** Creates the fade with per-call settings assigned before Construct. TimeBeforeKill <= 0 uses automatic removal. */
 UFUNCTION(BlueprintCallable, Category="Fade", meta=(DisplayName="Fade Configured"))
 UUserWidget* FadeConfigured(double TimeBeforeKill=0, double HoldDurationUntilFade=0, double FadeDuration=1.5, bool DisableFade=false, bool ReverseFadeOnAppear=false, int32 ZOrder=100);

	/** Only for direct PIE entry. Explicit stage travel and save loading take priority. Ignored outside PIE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	bool bOverrideStageForTesting = false;

	/** The stage to force while bOverrideStageForTesting is on (e.g. "Stage_Bar_Intro"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage", meta = (EditCondition = "bOverrideStageForTesting"))
	FName TestStage = NAME_None;

	/**
	 * The resolved stage for this level. Pure and order-independent - safe to call from any actor's
	 * BeginPlay (or anywhere) without worrying about who initialised first.
	 */
	UFUNCTION(BlueprintPure, Category = "Stage")
	FName GetActiveStage() const;

	/**
	 * The checkpoint this level was restored from (death-screen Retry, Continue), or None for a normal
	 * start. Valid already in OnStageBegin: branch on it to skip intros / sequences the player has
	 * already seen and set the level up for that point instead.
	 */
	UFUNCTION(BlueprintPure, Category = "Stage")
	FName GetResumeCheckpoint() const;

	/** Implement in your BPLM child to run the Chain-A / Chain-B logic for the resolved stage. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Stage")
	void OnStageBegin(FName Stage);

	/**
	 * "Hard initiate": the name of a parameterless Custom Event in this BPLM that sets the level up.
	 *
	 * It is called exactly once per level entry, on every way in, and only after any save restore for
	 * that entry has finished - so lighting, material collections and actor placement done there
	 * cannot be overwritten by the restore a tick later. It stays an ordinary Custom Event you own:
	 * name it what you like, wire it where you like, and call it yourself as well.
	 *
	 * This exists because OnStageBegin is not reliable for setup. A checkpoint resume skips it
	 * entirely (see bEncounterResumePending in BeginPlay), which silently takes the whole chain hanging
	 * off it - scene basics included - out of a loaded game.
	 *
	 * Leave it None to opt out; every BPLM that has not made such an event behaves exactly as before.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FName HardInitiateEvent;

	/**
	 * Run the hard initiate now. Called for you at the right moment; exposed so you can also drive it
	 * by hand - re-running setup after a scene change, or testing it without a save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	bool RunHardInitiate();

	/** True when this level entry came from a save (a plain load or a checkpoint resume). */
	UFUNCTION(BlueprintPure, Category = "Stage")
	bool DidEnterFromLoad() const { return bEnteredFromLoad; }

	// ---- Sub-stages -------------------------------------------------------------------------------
	// A sub-stage is a named checkpoint INSIDE one stage: "the player has got this far, rebuild the
	// scene from here". The stage says which chapter; the sub-stage says where in it.
	//
	// The whole system is three things:
	//   1. Story Checkpoint("<name>")  - call it where the player reaches that point. It records the
	//                                    name and asks for a story autosave.
	//   2. Get Sub Stage               - switch on it INSIDE that stage's branch of the hard initiate.
	//   3. that switch's Default pin   - everything else: no sub-stage, an old one from another
	//                                    stage, a name nobody wrote a case for. All of them fall
	//                                    through to the stage's normal setup, which is why putting
	//                                    the switch inside the stage case is not optional.
	//
	// It rides in the save as FVTGLevelProgressRecord::SubStage, restored at the top of BeginPlay -
	// before the hard initiate runs, which is the only ordering that matters here.

	/**
	 * The story checkpoint this level entry should rebuild from, or None for the stage's normal setup.
	 *
	 * Read it with a Switch on Name placed inside the stage's own branch, never instead of the stage
	 * switch: a sub-stage written during a different stage must not match anything here, and nesting
	 * it is what guarantees that without anyone having to remember to clear it.
	 */
	UFUNCTION(BlueprintPure, Category = "Stage")
	FName GetSubStage() const { return SubStage; }

	/**
	 * "The player has reached this point." Records SubStage and asks for a story autosave.
	 *
	 * One node, one name, both effects - because they are the same authoring decision: a place worth
	 * saving at is a place worth being able to rebuild. The name is also the autosave's beat, so two
	 * checkpoints must not share one (the coordinator refuses a repeat of the last beat, by design).
	 * Prefix it with the stage, e.g. "L5.Corridor", and collisions stop being possible.
	 *
	 * Leaving SubStage as it was and only saving is a legitimate thing to want - give those points
	 * their own names and wire their switch pins to the same setup chain.
	 *
	 * Returns false when nothing was scheduled: no name, or the coordinator turned it down (same beat
	 * again, a load in flight, or an editor run with VTG.AutoSaveInDevRuns off). Never fails silently:
	 * every refusal the coordinator makes is logged by it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	bool StoryCheckpoint(FName InSubStage);

	/** Forget the checkpoint and go back to the stage's normal setup. Rarely needed - the nested
	 *  switch already ignores a sub-stage belonging to another stage. */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	void ClearSubStage();

	// ---- Blocking manual saves across a story section ---------------------------------------------

	/**
	 * Close (or reopen) the terminal's save button for a stretch of story. Drop one of these at the
	 * start of a section and the matching one at the end.
	 *
	 * What it is for: a section whose setup cannot be rebuilt from an arbitrary moment. The alley is
	 * the case it was written for - it destroys a lot of actors, and a destroyed actor is recorded in
	 * the save and destroyed again on load, which no amount of hard-initiate setup can undo. Blocking
	 * the save is cheaper and safer than making every one of those destructions reversible.
	 *
	 * What it does NOT touch: the combat checkpoint. SaveEncounterCheckpoint writes through
	 * SaveCheckpoint -> AutoSave and never consults CanSaveNow, so dying mid-section still works
	 * exactly as before. The story autosave does consult it, so it waits rather than writing - which
	 * is the intent, and it gives up after StoryAutoSaveTimeout if the section runs long.
	 *
	 * It is runtime-only state. It is a C++ field, and the progress snapshot only collects the
	 * Blueprint variables you declared, so it can never be written into a save and come back stuck:
	 * every level entry starts open. The one thing to remember is the other direction - a restored
	 * sub-stage that drops the player back INSIDE such a section has to block again, so put a call in
	 * that sub-stage's branch of the hard initiate too.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Story", meta = (DisplayName = "Set Manual Save Blocked"))
	void SetManualSaveBlocked(bool bBlocked);

	/** True while this manager is holding the terminal's save button closed. */
	UFUNCTION(BlueprintPure, Category = "Save|Story")
	bool IsManualSaveBlocked() const { return bManualSaveBlocked; }

 /** Only story entries recorded in a loaded save are suppressed. */
 UFUNCTION(BlueprintCallable, Category="Save|Story")
 bool EnterSavedStoryEvent(FName EventId);
 bool HasRestoredLevelProgress() const { return bProgressRestored; }
 void CaptureLevelProgress(FVTGLevelProgressRecord& Record) const;
 void RestoreLevelProgress(const FVTGLevelProgressRecord& Record);

 /** Exclude temporary Blueprint values from automatic progress snapshots. Object references are always excluded. */
 UPROPERTY(EditDefaultsOnly, Category="Save|Progress")
 TSet<FName> ExcludedProgressVariables;

 /**
  * Stable key for this manager's progress in a save. Leave it empty and the Actor's name is used -
  * which is what every existing BPLM does.
  *
  * Renaming one in the Outliner is safe: that writes ActorLabel and never calls Rename, so GetFName()
  * - and this key - does not change. What does break the key is DELETING the actor and placing a
  * replacement, which gets a fresh auto-generated name, or duplicating one. Set this once (anything
  * unique, e.g. "L5.Clinic") and even that becomes free. UVTGSaveKeyWatcher warns at the moment of
  * deletion when a manager with no id here is removed.
  */
 UPROPERTY(EditAnywhere, Category="Save|Progress")
 FName LevelManagerSaveId;

 /** LevelManagerSaveId when set, otherwise the Actor name - the key the save is written under. */
 FName GetProgressKey() const { return LevelManagerSaveId.IsNone() ? GetFName() : LevelManagerSaveId; }

 /** Call after pre-fight dialogue, before enabling enemies. ResumeEvent must be a parameterless event on this BPLM. */
 UFUNCTION(BlueprintCallable, Category="Save|Combat")
 bool SaveEncounterCheckpoint(FName CheckpointId, FName ResumeEvent, const FTransform& RespawnTransform, int32 Order = 0);

 /** Change the rebuild entry used by subsequent manual/automatic saves; does not overwrite the pre-fight autosave. */
 UFUNCTION(BlueprintCallable, Category="Save|Combat")
 bool SetEncounterResumeEvent(FName ResumeEvent);

 UFUNCTION(BlueprintPure, Category="Save|Combat")
 bool IsResumingEncounter() const { return bDispatchingEncounterResume; }

 /** Called once after player, Narrative and inventory restoration. Also available for isolated verification. */
 void ResumeSavedEncounter();

 /** Returns true while deferring this parameterless entry until Narrative has fully exited. */
 UFUNCTION(BlueprintCallable, Category="Story|Combat")
 bool DeferEncounterUntilDialogueEnds(class UNarrativeComponent* Narrative, FName ResumeEvent);

private:
 TWeakObjectPtr<class UNarrativeComponent> DeferredEncounterNarrative;
 FName DeferredEncounterEvent;
 bool bDeferredEncounterQueued = false;
 UFUNCTION() void HandleDeferredEncounterDialogueFinished(class UDialogue* Dialogue, bool bStartingNewDialogue);
 void ResumeDeferredEncounterWhenIdle();
 bool bProgressRestored = false;
 TSet<FName> StartedStoryEvents;
 TSet<FName> RestoredStoryEvents;
 FName EncounterResumeEvent;
 FName EncounterCheckpointId;
 bool bEncounterResumePending = false;
 bool bDispatchingEncounterResume = false;
 UFUNCTION() void HandleEncounterSlotLoaded(int32 Slot);
 /** One shot, whichever path gets there first. */
 void FireLevelReady(bool bFromLoad);
 /** Safety net for a restore that ends without ever broadcasting, so setup is never simply lost. */
 void PollLevelReadyAfterRestore();
 bool bLevelReadyFired = false;
 bool bEnteredFromLoad = false;
 /** Visible in the actor's details while the game runs, so "which checkpoint does it think it is in"
  *  is answerable without reading the log. Written only by StoryCheckpoint and the save restore. */
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Stage", meta = (AllowPrivateAccess = "true"))
 FName SubStage;
 /** Runtime only, and deliberately so - see SetManualSaveBlocked. Visible in the details panel while
  *  the game runs so "why can't I save" is answerable without reading the log. */
 UPROPERTY(VisibleInstanceOnly, Category = "Save|Story", meta = (AllowPrivateAccess = "true"))
 bool bManualSaveBlocked = false;

protected:

	virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
