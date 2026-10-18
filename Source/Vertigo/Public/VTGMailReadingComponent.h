#pragma once
#include "Components/ActorComponent.h"
#include "VTGMailReadingComponent.generated.h"

/** Holds the mail cleanup continuation across a dialogue -> story Note handoff. No input ownership. */
UCLASS()
class VERTIGO_API UVTGMailReadingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    static UVTGMailReadingComponent* GetOrCreate(AActor* Reader);
    static UVTGMailReadingComponent* Find(AActor* Reader);
    void SetHold(UObject* Source, bool bHold);
    void NoteOpened(UObject* Note);
    void NoteClosed(UObject* Note);

    /** True means the cleanup has been queued and must not run from this event. */
    UFUNCTION(BlueprintCallable, Category="Mail", meta=(DefaultToSelf="Reader"))
    static bool DeferMailReadingFinish(AActor* Reader, FName CleanupEvent);

    /** Restore the mail view only while this reader is still the controller's view target. */
    UFUNCTION(BlueprintPure, Category="Mail", meta=(DefaultToSelf="Reader"))
    static bool CanRestoreMailViewTarget(AActor* Reader);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    TSet<TWeakObjectPtr<UObject>> Holds;
    TWeakObjectPtr<UObject> ActiveNote;
    FName PendingCleanup;
    void ResumeIfReady();
};
