#include "VTGSequenceAudioSystem.h"
#include "Audio/VTGCutsceneAudio.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "IMovieScenePlayer.h"
#include "LevelSequence.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Systems/FloatChannelEvaluatorSystem.h"

namespace
{
using namespace UE::MovieScene;
struct FApplySequenceAudioGain
{
    FInstanceRegistry* Registry;
    explicit FApplySequenceAudioGain(FInstanceRegistry* InRegistry):Registry(InRegistry){}
    void ForEachEntity(FInstanceHandle Handle,const FMovieSceneAudioComponentData& Audio,FFrameTime Time,double& Volume) const
    {
        if(!Audio.Section)return;
        const FSequenceInstance& Instance=Registry->GetInstance(Handle);
        auto* Player=Instance.GetPlayer();
        if(!Player)return;
        auto& Template=Player->GetEvaluationTemplate();
        auto ID=Instance.GetSequenceID();
        double Gain=1.;
        for(;;)
        {
            if(auto* Sequence=Cast<ULevelSequence>(Template.GetSequence(ID)))
                if(auto* Settings=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Sequence))
                {
                    const float Value=Settings->SequenceAudioVolumeMultiplier;
                    Gain*=FMath::IsFinite(Value)?FMath::Max(0.f,Value):1.f;
                }
            if(ID==MovieSceneSequenceID::Root)break;
            const auto* Hierarchy=Template.GetHierarchy();
            const auto* Node=Hierarchy?Hierarchy->FindNode(ID):nullptr;
            if(!Node)break;
            ID=Node->ParentID;
        }
        // Re-evaluate the authored value: constant channels may be cached by Sequencer.
        // Multiplying the previous result would compound the gain on every frame.
        Volume=Audio.Section->GetSoundVolume(Time)*Gain;
    }
};
auto GainTask()
{
    auto* BuiltIn=FBuiltInComponentTypes::Get();
    return FEntityTaskBuilder()
        .Read(BuiltIn->InstanceHandle)
        .Read(FMovieSceneTracksComponentTypes::Get()->Audio)
        .Read(BuiltIn->EvalTime)
        .Write(BuiltIn->DoubleResult[0])
        .SetDesiredThread(ENamedThreads::GameThread)
        .FilterNone({BuiltIn->Tags.Ignored});
}
}

UVTGSequenceAudioSystem::UVTGSequenceAudioSystem(const FObjectInitializer& ObjectInitializer)
    :Super(ObjectInitializer)
{
    using namespace UE::MovieScene;
    RelevantComponent=FMovieSceneTracksComponentTypes::Get()->Audio;
    Phase=ESystemPhase::Scheduling;
    if(HasAnyFlags(RF_ClassDefaultObject))
    {
        DefineImplicitPrerequisite(UFloatChannelEvaluatorSystem::StaticClass(),GetClass());
        DefineComponentProducer(GetClass(),FBuiltInComponentTypes::Get()->DoubleResult[0]);
    }
}
void UVTGSequenceAudioSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* Scheduler)
{
    GainTask().Fork_PerEntity<FApplySequenceAudioGain>(&Linker->EntityManager,Scheduler,Linker->GetInstanceRegistry());
}
void UVTGSequenceAudioSystem::OnRun(FSystemTaskPrerequisites& Prerequisites,FSystemSubsequentTasks& Subsequents)
{
    GainTask().Dispatch_PerEntity<FApplySequenceAudioGain>(&Linker->EntityManager,Prerequisites,&Subsequents,Linker->GetInstanceRegistry());
}
