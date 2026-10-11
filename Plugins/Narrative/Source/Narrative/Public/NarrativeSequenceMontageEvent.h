#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSequenceMontageEvent.generated.h"
class ULevelSequencePlayer;
class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;
USTRUCT()
struct FNarrativeSavedMontage
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UAnimMontage> Montage;
 TWeakObjectPtr<USkeletalMeshComponent> Mesh;
 TWeakObjectPtr<UAnimInstance> Anim;
 float Position = 0.f;
 float Rate = 1.f;
 bool bPlaying = true;
 TArray<int32> NextSections;
};
/** Shared by sequence sessions so overlapping sequences only restore after the last one ends. */
UCLASS()
class NARRATIVE_API UNarrativeSequenceMontageGuard : public UActorComponent
{
 GENERATED_BODY()
public:
 UNarrativeSequenceMontageGuard();
 void Begin(ULevelSequencePlayer* Player);
 static void Prepare(ULevelSequencePlayer* Player);
 virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TArray<FNarrativeSavedMontage> Saved;
 TArray<TWeakObjectPtr<ULevelSequencePlayer>> Players;
 void Restore();
};

