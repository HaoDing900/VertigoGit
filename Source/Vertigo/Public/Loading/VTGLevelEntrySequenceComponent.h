#pragma once
#include "Components/ActorComponent.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "VTGLevelEntrySequenceComponent.generated.h"

class ULevelSequence;
class ALevelSequenceActor;
class UMovieSceneSequencePlayer;
class UVTGMediaLoadingPageSystem;
class UVTGSaveCoordinator;

UENUM(BlueprintType)
enum class EVTGEntryStartPolicy : uint8
{
    Immediate,
    WithLoadingReveal UMETA(DisplayName="When Loading Starts Revealing"),
    AfterLoadingReveal UMETA(DisplayName="After Loading Screen Disappears")
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVTGEntryStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGEntryFinished, bool, Skipped);

/** Add to a level manager. Own a configured sequence, or adopt an existing player without changing its callbacks. */
UCLASS(ClassGroup=(Story), meta=(BlueprintSpawnableComponent, DisplayName="VTG Level Entry Sequence"))
class VERTIGO_API UVTGLevelEntrySequenceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVTGLevelEntrySequenceComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Entry Sequence")
    TObjectPtr<ULevelSequence> EntrySequence;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Entry Sequence")
    EVTGEntryStartPolicy StartPolicy=EVTGEntryStartPolicy::WithLoadingReveal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Entry Sequence")
    bool bPlayOnBeginPlay=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Entry Sequence")
    bool bSkipOnSaveResume=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Entry Sequence")
    FMovieSceneSequencePlaybackSettings PlaybackSettings;
    UPROPERTY(BlueprintAssignable, Category="Entry Sequence") FVTGEntryStarted OnStarted;
    /** Natural completion or explicit SkipEntry. Skipped=true also reports save-resume suppression. */
    UPROPERTY(BlueprintAssignable, Category="Entry Sequence") FVTGEntryFinished OnFinished;
    UFUNCTION(BlueprintCallable, Category="Entry Sequence") bool PlayEntry();
    /** Create without evaluating or playing, allowing existing graphs to bind callbacks before requesting entry. */
    UFUNCTION(BlueprintCallable, Category="Entry Sequence") UMovieSceneSequencePlayer* CreateEntryPlayer();
    /** For existing graphs that already create a player and bind their own finish/skip handling. */
    UFUNCTION(BlueprintCallable, Category="Entry Sequence") bool PlayEntryPlayer(UMovieSceneSequencePlayer* InPlayer);
    /** Cancel a pending entry, or jump a playing sequence to its end and perform its normal finish evaluation. */
    UFUNCTION(BlueprintCallable, Category="Entry Sequence") void SkipEntry();
    UFUNCTION(BlueprintPure, Category="Entry Sequence") UMovieSceneSequencePlayer* GetEntryPlayer() const { return Player; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend bool VTGTestEntryComponent();
    UPROPERTY(Transient) TObjectPtr<UMovieSceneSequencePlayer> Player;
    UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> OwnedActor;
    UPROPERTY(Transient) TObjectPtr<UVTGMediaLoadingPageSystem> Loading;
    UPROPERTY(Transient) TObjectPtr<UVTGSaveCoordinator> Save;
    bool bRequested=false;
    bool bStarted=false;
    bool bSkipping=false;
    bool bResumingSave=false;
    uint32 RequestSerial=0;
    FTimerHandle AutoPlayTimer;
    bool IsSaveResume() const;
    void StartNow();
    void UnbindLoading();
    void OnReveal();
    void OnHidden();
    void OnCancelled();
    void Finish(bool Skipped);
    UFUNCTION() void OnSequenceFinished();
    UFUNCTION() void OnSequenceStopped();
    UFUNCTION() void OnSlotLoaded(int32 Slot);
};
