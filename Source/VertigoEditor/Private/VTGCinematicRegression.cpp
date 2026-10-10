#include "VTGCinematicRegressionFixture.h"
#include "UI/VTGSequenceSkipSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "UObject/UnrealType.h"

int32 UVTGSequenceRegressionDirector::TriggerCount = 0;
int32 UVTGSequenceRegressionDirector::RepeatCount = 0;
int32 UVTGSequenceRegressionDirector::FinishCount = 0;
bool UVTGSequenceRegressionDirector::bPauseAtEvent = false;
TWeakObjectPtr<UNarrativeComponent> UVTGSequenceRegressionDirector::DialogueOwner;

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGCinematicRegression, "Vertigo.Dialogue.CinematicRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGCinematicRegression::RunTest(const FString&)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);

    AActor* Owner = World->SpawnActor<AActor>();
    auto* Component = NewObject<UNarrativeComponent>(Owner);
    Owner->AddInstanceComponent(Component);
    Component->RegisterComponent();
    auto* Dialogue = NewObject<UVTGReturnRegressionDialogue>(Component);
    auto* Back = NewObject<UDialogueNode_Player>(Dialogue);
    auto* Parent = NewObject<UDialogueNode_NPC>(Dialogue);
    auto* Choice = NewObject<UDialogueNode_Player>(Dialogue);
    Choice->Line.Text = FText::FromString(TEXT("Read another letter"));
    Parent->PlayerReplies = {Choice};
    Back->NPCReplies = {Parent};
    Dialogue->Prepare(Component, Back);
    Component->CurrentDialogue = Dialogue;
    TestTrue(TEXT("Silent back option accepted"), Dialogue->SelectDialogueOption(Back));
    TestEqual(TEXT("Parent choices survive the old selection notification"), Dialogue->VisibleChoices, 1);
    Component->CurrentDialogue = nullptr;

    auto* Sequence = NewObject<ULevelSequence>();
    Sequence->Initialize();
    auto* DirectorProperty = FindFProperty<FClassProperty>(ULevelSequence::StaticClass(), TEXT("DirectorClass"));
    DirectorProperty->SetObjectPropertyValue_InContainer(Sequence, UVTGSequenceRegressionDirector::StaticClass());
    auto* Scene = Sequence->GetMovieScene();
    Scene->SetDisplayRate(FFrameRate(30,1));
    Scene->SetTickResolutionDirectly(FFrameRate(30,1));
    Scene->SetPlaybackRange(0, 30);
    auto* Track = Scene->AddTrack<UMovieSceneEventTrack>();
    auto* Triggers = NewObject<UMovieSceneEventTriggerSection>(Track);
    Triggers->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(30)));
    Track->AddSection(*Triggers);
    FMovieSceneEvent Event;
    Event.Ptrs.Function = UVTGSequenceRegressionDirector::StaticClass()->FindFunctionByName(TEXT("Trigger"));
    for (int32 Frame : {5,15,29}) Triggers->EventChannel.GetData().AddKey(FFrameNumber(Frame), Event);
    auto* RepeatTrack = Scene->AddTrack<UMovieSceneEventTrack>();
    auto* Repeater = NewObject<UMovieSceneEventRepeaterSection>(RepeatTrack);
    Repeater->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(30)));
    Repeater->Event.Ptrs.Function = UVTGSequenceRegressionDirector::StaticClass()->FindFunctionByName(TEXT("Repeat"));
    RepeatTrack->AddSection(*Repeater);
    ALevelSequenceActor* Actor = nullptr;
    auto* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, FMovieSceneSequencePlaybackSettings(), Actor);
    if (TestNotNull(TEXT("Sequence player created"), Player))
    {
        auto* Observer = NewObject<UVTGSequenceRegressionDirector>(World);
        Player->OnFinished.AddDynamic(Observer, &UVTGSequenceRegressionDirector::Finished);
        UVTGSequenceRegressionDirector::TriggerCount = UVTGSequenceRegressionDirector::RepeatCount = UVTGSequenceRegressionDirector::FinishCount = 0;
        Player->Play();
        for (int32 i = 0; i < 40 && UVTGSequenceSkipSubsystem::AdvanceFrame(Player); ++i) {}
        TestEqual(TEXT("Beginning, middle and last-frame triggers execute once"), UVTGSequenceRegressionDirector::TriggerCount, 3);
        TestTrue(TEXT("Repeaters evaluated throughout skip"), UVTGSequenceRegressionDirector::RepeatCount >= 29);
        TestEqual(TEXT("Normal completion broadcasts once"), UVTGSequenceRegressionDirector::FinishCount, 1);
        TestFalse(TEXT("Skip finishes playback"), Player->IsPlaying());

        UVTGSequenceRegressionDirector::TriggerCount = 0;
        UVTGSequenceRegressionDirector::bPauseAtEvent = true;
        Player->Play();
        for (int32 i = 0; i < 40 && UVTGSequenceSkipSubsystem::AdvanceFrame(Player); ++i) {}
        TestEqual(TEXT("Pause event prevents later events being skipped over"), UVTGSequenceRegressionDirector::TriggerCount, 1);
        TestTrue(TEXT("Event pause preserved"), Player->IsPaused());
        UVTGSequenceRegressionDirector::bPauseAtEvent = false;
        Player->Play();
        for (int32 i = 0; i < 40 && UVTGSequenceSkipSubsystem::AdvanceFrame(Player); ++i) {}
        TestEqual(TEXT("Resume does not repeat already executed trigger"), UVTGSequenceRegressionDirector::TriggerCount, 3);
        UVTGSequenceRegressionDirector::TriggerCount = 0;
        UVTGSequenceRegressionDirector::DialogueOwner = Component;
        Player->Play();
        for (int32 i = 0; i < 40 && UVTGSequenceSkipSubsystem::AdvanceFrame(Player); ++i) {}
        TestEqual(TEXT("Dialogue start interrupts skip before later story events"), UVTGSequenceRegressionDirector::TriggerCount, 1);
        TestTrue(TEXT("Authored sequence remains playing normally; no artificial pause deadlock"), Player->IsPlaying());
        TestNotNull(TEXT("New dialogue remains available"), Component->GetCurrentDialogue());
        const FFrameTime Boundary = Player->GetCurrentTime().Time;
        TestFalse(TEXT("Active dialogue blocks another skip before options are visible"), UVTGSequenceSkipSubsystem::AdvanceFrame(Player));
        TestEqual(TEXT("Dialogue guard leaves sequence position unchanged"), Player->GetCurrentTime().Time, Boundary);
        if (Component->CurrentDialogue)
        {
            CastChecked<UVTGReturnRegressionDialogue>(Component->GetCurrentDialogue())->WaitForChoice();
            TestFalse(TEXT("Waiting for a choice blocks skipping too"), UVTGSequenceSkipSubsystem::AdvanceFrame(Player));
        }
        Component->CurrentDialogue = nullptr;
        UVTGSequenceRegressionDirector::DialogueOwner.Reset();
        for (int32 i = 0; i < 40 && UVTGSequenceSkipSubsystem::AdvanceFrame(Player); ++i) {}
        TestEqual(TEXT("After completing dialogue, a new skip preserves remaining events"), UVTGSequenceRegressionDirector::TriggerCount, 3);
        Player->Stop();
    }
    World->DestroyWorld(false);
    return true;
}
#endif
