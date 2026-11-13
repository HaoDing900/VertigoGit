#include "Audio/VTGCutsceneAudio.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSequenceAudioGainTest,"Vertigo.Sequence.AudioVolumeMultiplier",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSequenceAudioGainTest::RunTest(const FString&)
{
    using namespace UE::MovieScene;
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SequenceAudioGainTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto MakeSequence=[&](){auto* S=NewObject<ULevelSequence>(World);S->Initialize();S->GetMovieScene()->SetDisplayRate(FFrameRate(24,1));S->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(24,1));S->GetMovieScene()->SetPlaybackRange(0,240);return S;};
    auto* Root=MakeSequence();auto* Child=MakeSequence();
    auto* Sound=NewObject<USoundWave>(World);Sound->Duration=10.f;
    auto AddAudio=[&](ULevelSequence* S,float Base)
    {
        auto* Track=S->GetMovieScene()->AddTrack<UMovieSceneAudioTrack>();
        auto* Section=CastChecked<UMovieSceneAudioSection>(Track->CreateNewSection());
        Section->SetSound(Sound);Section->SetRange(TRange<FFrameNumber>(0,240));Track->AddSection(*Section);
        Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0)->SetDefault(Base);
        return Section;
    };
    auto* RootAudio=AddAudio(Root,.8f);auto* ChildAudio=AddAudio(Child,.4f);
    auto* SubTrack=Root->GetMovieScene()->AddTrack<UMovieSceneSubTrack>();
    auto* Sub=NewObject<UMovieSceneSubSection>(SubTrack);Sub->SetSequence(Child);Sub->SetRange(TRange<FFrameNumber>(0,240));SubTrack->AddSection(*Sub);
    auto* Settings=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Root,true);
    auto* ChildSettings=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Child,true);
    TestEqual(TEXT("Default asset gain"),Settings->SequenceAudioVolumeMultiplier,1.f);
    ALevelSequenceActor* Actor=nullptr;
    auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Root,FMovieSceneSequencePlaybackSettings(),Actor);
    if(!TestNotNull(TEXT("Sequence player"),Player)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
    auto Check=[&](int32 Frame,double ExpectedRoot,double ExpectedChild)
    {
        Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(Frame),EUpdatePositionMethod::Jump));
        auto* Linker=Player->GetEvaluationTemplate().GetEntitySystemLinker();
        int32 Found=0;
        FEntityTaskBuilder().Read(FMovieSceneTracksComponentTypes::Get()->Audio).Read(FBuiltInComponentTypes::Get()->DoubleResult[0])
            .Iterate_PerEntity(&Linker->EntityManager,[&](const FMovieSceneAudioComponentData& Data,double Volume)
            {
                if(Data.Section==RootAudio){++Found;TestTrue(TEXT("Root audio gain"),FMath::IsNearlyEqual(Volume,ExpectedRoot,.0001));}
                if(Data.Section==ChildAudio){++Found;TestTrue(TEXT("Nested audio gain"),FMath::IsNearlyEqual(Volume,ExpectedChild,.0001));}
            });
        TestEqual(TEXT("Both audio tracks evaluated"),Found,2);
    };
    Check(1,.8,.4);
    Settings->SequenceAudioVolumeMultiplier=2.f;ChildSettings->SequenceAudioVolumeMultiplier=.5f;
    Check(2,1.6,.4);Check(3,1.6,.4); // Constant channels must never compound.
    Settings->SequenceAudioVolumeMultiplier=0.f;Check(4,0.,0.);
    Settings->SequenceAudioVolumeMultiplier=1.f;Check(5,.8,.2); // Restore after zero.
    // Animated authored curves remain intact and are multiplied at the current frame.
    auto* Curve=RootAudio->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
    Curve->AddLinearKey(0,.2f);Curve->AddLinearKey(24,.6f);
    Root->MarkAsChanged();Settings->SequenceAudioVolumeMultiplier=2.f;
    Check(12,.8,.4);
    TestTrue(TEXT("Authored curve unchanged"),FMath::IsNearlyEqual(RootAudio->GetSoundVolume(FFrameTime(12)),.4f));
    Player->Stop();Player->Play();Check(13,RootAudio->GetSoundVolume(FFrameTime(13))*2.,.4);
    Player->Stop();
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
