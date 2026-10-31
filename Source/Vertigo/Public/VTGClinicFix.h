#pragma once
#include "CoreMinimal.h"
#include "NarrativeEvent.h"
#include "Components/ActorComponent.h"
#include "VTGClinicFix.generated.h"
class UDialogue;
class UAnimInstance;
class UAnimMontage;
class UNarrativeComponent;

UCLASS()
class VERTIGO_API UVTGNarrativeMontageCleanup : public UActorComponent
{
 GENERATED_BODY()
public:
 void Track(UObject* Context, UAnimInstance* Anim, UAnimMontage* Montage);
 void TrackRestored(UAnimInstance* Anim, UAnimMontage* Montage);
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 struct FClip { TWeakObjectPtr<UAnimInstance> Anim; TWeakObjectPtr<UAnimMontage> Montage; int32 InstanceID; };
 TArray<FClip> Clips;
 TWeakObjectPtr<UNarrativeComponent> Narrative;
 TWeakObjectPtr<UDialogue> SourceDialogue;
 FTimerHandle CleanupTimer;
 void Cleanup();
 UFUNCTION() void DialogueFinished(UDialogue* Dialogue, bool StartingNewDialogue);
};

/** Compatibility type for removing the former Clinic-only event from old assets. Narrative now restores Tick. */
UCLASS(DisplayName="Legacy Clinic Camera Event (Unused)")
class VERTIGO_API UVTGClinicLobbyCameraEvent : public UNarrativeEvent
{
 GENERATED_BODY()
public:
 virtual void ExecuteEvent_Implementation(APawn*, APlayerController*, UNarrativeComponent* Component) override;
};

UCLASS(DisplayName="Clinic Enable Exam Button")
class VERTIGO_API UVTGClinicButtonEvent : public UNarrativeEvent
{
 GENERATED_BODY()
public:
 virtual void ExecuteEvent_Implementation(APawn*, APlayerController*, UNarrativeComponent* Component) override;
};
