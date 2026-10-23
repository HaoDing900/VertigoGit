#pragma once
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGNarrativeInsert.generated.h"

class UAnimInstance;
class UAnimMontage;
class ACharacter;

/** One owner for an entire chain of dialogue inserts, including across event instances. */
UCLASS()
class VERTIGO_API UVTGNarrativeInsertState : public UActorComponent
{
    GENERATED_BODY()
public:
    bool Play(ACharacter* Character, UAnimMontage* Clip, bool Instant = false);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UAnimInstance> Anim;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> Insert;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> Base;
    float BasePosition = 0.f;
    float BaseRate = 1.f;
    bool BasePlaying = false;
    bool Replacing = false;
    TArray<int32> BaseNextSections;
    UFUNCTION() void OnEnded(UAnimMontage* Montage, bool Interrupted);
    void Finish(bool Restore);
};

UCLASS()
class VERTIGO_API UVTGNarrativeInsertLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Narrative|Animation", meta=(WorldContext="WorldContextObject"))
    static void PlayNarrativeInsert(UObject* WorldContextObject, FName TargetIdentity, UAnimMontage* InterruptMontage, bool Instant = false);

    /** Per-call hard cut into a montage; leaves the montage asset and other slot groups unchanged. */
    UFUNCTION(BlueprintCallable, Category="Narrative|Animation")
    static float PlayNarrativeMontage(ACharacter* Character, UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName = NAME_None, bool Instant = false);
};
