#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "VTGSequenceDialogue.generated.h"
class ALevelSequenceActor;
class ULevelSequencePlayer;
class ULevelSequence;

/** One interruption per playback, never a dialogue lock for the duration of a cutscene. */
UCLASS()
class VERTIGO_API UVTGSequenceDialogueGate : public UActorComponent
{
    GENERATED_BODY()
public:
    static void Install(ALevelSequenceActor* Actor);
    static const FName KeepDialogueTag;
    static const FName EndDialogueTag;
    static bool ShouldEndDialogue(const ALevelSequenceActor* Actor);
    bool IsPlaybackActive() const { return bPlaybackActive; }
    static bool GetSequenceDefault(ULevelSequence* Sequence);
    static void SetSequenceDefault(ULevelSequence* Sequence,bool Enabled);
private:
    friend class UVTGSequenceDialogueEntrySystem;
    bool bPlaybackActive=false;
    TWeakObjectPtr<ULevelSequencePlayer> BoundPlayer;
    UFUNCTION() void OnPlay();
    UFUNCTION() void OnPlaybackEnded();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};

/** Runs before even the earliest Sequence event / camera spawn on its first evaluation. */
UCLASS()
class VERTIGO_API UVTGSequenceDialogueEntrySystem : public UMovieSceneEntitySystem
{
    GENERATED_BODY()
public:
    UVTGSequenceDialogueEntrySystem(const FObjectInitializer& Init);
private:
    virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override {return true;}
    virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites,FSystemSubsequentTasks& Subsequents) override;
};

/** Binds placed actors before Auto Play and runtime actors before their caller starts playback. */
UCLASS()
class VERTIGO_API UVTGSequenceDialogueSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void PostInitialize() override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
private:
    FDelegateHandle SpawnHandle;
    FDelegateHandle LevelHandle;
    void BindAll();
    void OnActorSpawned(AActor* Actor);
    void OnLevelAdded(ULevel* Level,UWorld* World);
};
