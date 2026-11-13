#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "VTGSequenceAudioSystem.generated.h"

/** Applies asset gain before Sequencer submits audio, in editor previews and cooked games. */
UCLASS()
class UVTGSequenceAudioSystem : public UMovieSceneEntitySystem
{
    GENERATED_BODY()
public:
    UVTGSequenceAudioSystem(const FObjectInitializer& ObjectInitializer);
    virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* Scheduler) override;
    virtual void OnRun(FSystemTaskPrerequisites& Prerequisites,FSystemSubsequentTasks& Subsequents) override;
};
