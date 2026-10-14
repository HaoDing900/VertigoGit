#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGL2FightStateComponent.generated.h"

class UVTGSaveCoordinator;
class UNarrativeComponent;
class UDialogue;

/** Explicit gameplay handoff for the L2 street fight, after Sequencer restores its state. */
UCLASS(ClassGroup=(Story), meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGL2FightStateComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGL2FightStateComponent();
 UPROPERTY(EditAnywhere, Category="L2|After Fight")
 FTransform FemaleClientEndTransform;
 UPROPERTY(EditAnywhere, Category="L2|After Fight")
 FTransform PlayerEndTransform;

 UFUNCTION(BlueprintCallable, Category="L2|After Fight")
 void CompleteAfterFight(AActor* FemaleClient);
 UFUNCTION(BlueprintCallable, Category="L2|After Fight")
 void ResumeAfterFight(AActor* FemaleClient);

 // Also used by the isolated regression to verify the gameplay handoff without disk saves.
 void ApplyAfterFightState(bool bPlacePlayer);
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 friend struct FVTGL2FightRegression;
 UPROPERTY(Transient) TObjectPtr<AActor> Client;
 UPROPERTY(Transient) TObjectPtr<UVTGSaveCoordinator> Coordinator;
 UPROPERTY(Transient) TObjectPtr<UNarrativeComponent> Narrative;
 UPROPERTY(Transient) TObjectPtr<UDialogue> AfterFightDialogue;
 bool bWaitingForRestore = false;
 bool bCompletionApplied = false;
 UFUNCTION() void OnSlotLoaded(int32 Slot);
 UFUNCTION() void OnDialogueBegan(UDialogue* Dialogue);
 UFUNCTION() void OnDialogueFinished(UDialogue* Dialogue, bool bStartingNewDialogue);
 void BindNarrative();
 void FinishResume();
};
