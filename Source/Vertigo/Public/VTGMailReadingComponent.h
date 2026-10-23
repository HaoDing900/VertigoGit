#pragma once
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "VTGMailReadingComponent.generated.h"
class APlayerController;

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

    /** Invalidates only pending cleanup from a mail -> Note handoff. Does not move any camera. */
    UFUNCTION(BlueprintCallable, Category="Mail|Camera", meta=(DefaultToSelf="Reader"))
    static void NotifyMailReadingStarted(AActor* Reader);

    /** A successful Note -> mail dialogue bypasses ReadMailSafe; keep its normal finish cleanup enabled. */
    UFUNCTION(BlueprintCallable, Category="Mail|Camera", meta=(DefaultToSelf="Reader"))
    static void NotifyNoteMailDialogueStarted(AActor* Reader);

    /** Protected Note handoffs use a generation-checked delay; false retains the original BP Delay. */
    UFUNCTION(BlueprintCallable, Category="Mail|Camera", meta=(DefaultToSelf="Reader"))
    static bool ScheduleNoteMailCleanupStep(AActor* Reader, float Delay, FName Continuation, bool bFinalStep = false);

    /** Returns true when a Note-linked reading handled its camera return. */
    UFUNCTION(BlueprintCallable, Category="Mail|Camera", meta=(DefaultToSelf="Reader"))
    static bool RestoreNoteMailCamera(AActor* Reader);
    UFUNCTION(BlueprintCallable, Category="Mail|Camera", meta=(DefaultToSelf="Reader"))
    static void CaptureMailCameraReturn(AActor* Reader);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    TSet<TWeakObjectPtr<UObject>> Holds;
    TWeakObjectPtr<UObject> ActiveNote;
    FName PendingCleanup;
    uint64 ReadingGeneration = 0;
    TWeakObjectPtr<AActor> ReturnViewTarget;
    TWeakObjectPtr<APlayerController> ReturnController;
    TArray<TWeakObjectPtr<class UCameraComponent>> OriginalActiveCameras;
    bool bHasCameraSnapshot = false;
    bool bRestoredCamera = false;
    bool bProtectNoteCameraCleanup = false;
    TArray<FTimerHandle> CameraStepTimers;
    void ResumeIfReady();
};
