#include "VTGCinematicRegressionFixture.h"
#include "UI/VTGSequenceSkipSubsystem.h"
#include "UI/VTGSequenceSettings.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "UObject/UnrealType.h"
#include "Engine/GameInstance.h"

int32 UVTGSequenceRegressionDirector::TriggerCount = 0;
int32 UVTGSequenceRegressionDirector::RepeatCount = 0;
int32 UVTGSequenceRegressionDirector::FinishCount = 0;
bool UVTGSequenceRegressionDirector::bPauseAtEvent = false;
TWeakObjectPtr<UNarrativeComponent> UVTGSequenceRegressionDirector::DialogueOwner;
FSimpleDelegate UVTGSequenceRegressionDirector::TriggerCallback;

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
    TestFalse(TEXT("Existing sequence without settings defaults to No Skip"),UVTGSequenceSkipSubsystem::IsSkipAllowed(Sequence));
    auto* SkipSettings=NewObject<UVTGSequenceSettings>(Sequence);
    Sequence->AddAssetUserData(SkipSettings);
    TestFalse(TEXT("New settings default to No Skip"),UVTGSequenceSkipSubsystem::IsSkipAllowed(Sequence));
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
        const FFrameTime BeforeOptIn=Player->GetCurrentTime().Time;
        TestFalse(TEXT("No Skip blocks actual advancement"),UVTGSequenceSkipSubsystem::AdvanceFrame(Player));
        TestEqual(TEXT("No Skip leaves playback time unchanged"),Player->GetCurrentTime().Time,BeforeOptIn);
        SkipSettings->bAllowSkip=true;
        TestTrue(TEXT("Explicit opt-in allows skip"),UVTGSequenceSkipSubsystem::IsSkipAllowed(Sequence));
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

        // Only the root is opted in. Nested sequences are evaluated by its player,
        // including their own Directors, offsets and different display rates.
        auto* Root=NewObject<ULevelSequence>();Root->Initialize();
        auto* RootSettings=NewObject<UVTGSequenceSettings>(Root);RootSettings->bAllowSkip=true;Root->AddAssetUserData(RootSettings);
        Root->GetMovieScene()->SetDisplayRate(FFrameRate(24,1));
        Root->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(30,1));
        Root->GetMovieScene()->SetPlaybackRange(0,60);
        auto* Middle=NewObject<ULevelSequence>();Middle->Initialize();
        Middle->GetMovieScene()->SetDisplayRate(FFrameRate(30,1));
        Middle->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(30,1));
        Middle->GetMovieScene()->SetPlaybackRange(0,40);
        auto AddSub=[](ULevelSequence* Parent,ULevelSequence* Child,int32 Start,int32 End)
        {
            auto* SubTrack=Parent->GetMovieScene()->AddTrack<UMovieSceneSubTrack>();
            auto* Sub=NewObject<UMovieSceneSubSection>(SubTrack);Sub->SetSequence(Child);
            Sub->SetRange(TRange<FFrameNumber>(FFrameNumber(Start),FFrameNumber(End)));SubTrack->AddSection(*Sub);
        };
        AddSub(Root,Middle,10,50);AddSub(Middle,Sequence,5,35);
        SkipSettings->bAllowSkip=false;
        auto* NestedPlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Root,FMovieSceneSequencePlaybackSettings(),Actor);
        NestedPlayer->OnFinished.AddDynamic(Observer,&UVTGSequenceRegressionDirector::Finished);
        UVTGSequenceRegressionDirector::TriggerCount=UVTGSequenceRegressionDirector::RepeatCount=UVTGSequenceRegressionDirector::FinishCount=0;
        NestedPlayer->Play();
        for(int32 i=0;i<100 && UVTGSequenceSkipSubsystem::AdvanceFrame(NestedPlayer);++i){}
        TestEqual(TEXT("Two nested levels preserve all child trigger events without child skip opt-in"),UVTGSequenceRegressionDirector::TriggerCount,3);
        TestTrue(TEXT("Nested repeaters execute during root skip"),UVTGSequenceRegressionDirector::RepeatCount>0);
        TestEqual(TEXT("Nested root completes once"),UVTGSequenceRegressionDirector::FinishCount,1);
        TestFalse(TEXT("Nested root completes playback"),NestedPlayer->IsPlaying());
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSequenceSkipPromptRegression, "Vertigo.Dialogue.SequenceSkipPrompt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGSequenceSkipPromptRegression::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>(GEngine);GI->AddToRoot();GI->InitializeStandalone();
    auto* World=GI->GetWorld();auto* Skip=GI->GetSubsystem<UVTGSequenceSkipSubsystem>();check(World && Skip);
    using Phase=UVTGSequenceSkipSubsystem::EPhase;
    auto* Sequence=NewObject<ULevelSequence>();Sequence->Initialize();auto* Settings=NewObject<UVTGSequenceSettings>(Sequence);Settings->bAllowSkip=true;Sequence->AddAssetUserData(Settings);
    auto* Scene=Sequence->GetMovieScene();Scene->SetDisplayRate(FFrameRate(30,1));Scene->SetTickResolutionDirectly(FFrameRate(30,1));Scene->SetPlaybackRange(0,300);
    ALevelSequenceActor* Actor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Sequence,FMovieSceneSequencePlaybackSettings(),Actor);check(Player && Actor);
    auto* Observer=NewObject<UVTGSequenceRegressionDirector>(World);Player->OnFinished.AddDynamic(Observer,&UVTGSequenceRegressionDirector::Finished);UVTGSequenceRegressionDirector::FinishCount=0;
    Player->Play();Skip->TickAt(true,99);TestFalse(TEXT("A fresh cutscene does not show a prompt automatically"),Skip->bHintVisible);
    TestFalse(TEXT("An arbitrary key reveals without swallowing the key"),Skip->HandleKeyAt(EKeys::E,true,false,100));TestTrue(TEXT("Any-key prompt is visible"),Skip->bHintVisible);
    TestTrue(TEXT("Prompt uses the shared Inventory control texture"),Skip->SkipKeyBrush.GetResourceObject() && Skip->SkipKeyBrush.GetResourceObject()->GetPathName().Contains(TEXT("/Inventory/InventoryXBased/UMG/Textures/Controls/")));
    Skip->TickAt(true,102.3);TestTrue(TEXT("Prompt remains visible before the 2.4-second idle timeout"),Skip->bHintVisible);
    Skip->TickAt(true,102.5);TestFalse(TEXT("Prompt hides after 2.4 idle seconds"),Skip->bHintVisible);
    Skip->NotifyActivityAt(EKeys::LeftMouseButton,110);TestTrue(TEXT("Mouse button can reveal it again"),Skip->bHintVisible);
    Skip->TickAt(true,114.1);TestFalse(TEXT("Revealed prompt also times out"),Skip->bHintVisible);
    TestFalse(TEXT("Esc retains the pause path rather than starting a skip"),Skip->HandleKeyAt(EKeys::Escape,true,false,120));TestTrue(TEXT("Esc alone does not hold skip"),Skip->Phase==Phase::Idle);
    TestTrue(TEXT("Space begins a hold"),Skip->HandleKeyAt(EKeys::SpaceBar,true,false,200));const auto StartTime=Player->GetCurrentTime().Time;
    Skip->TickAt(true,200.4);TestTrue(TEXT("Hold progress reflects elapsed time"),FMath::IsNearlyEqual(Skip->HoldProgress,.5f,.001f));TestEqual(TEXT("Holding does not fast-forward"),Player->GetCurrentTime().Time,StartTime);
    TestTrue(TEXT("Early release is consumed"),Skip->HandleKeyAt(EKeys::SpaceBar,false,false,200.4));TestTrue(TEXT("Early release resets hold but retains the prompt"),Skip->Phase==Phase::Idle && Skip->bHintVisible && Skip->HoldProgress==0);
    Skip->TickAt(true,204.5);TestFalse(TEXT("Released hold prompt can time out"),Skip->bHintVisible);
    Skip->HandleKeyAt(EKeys::SpaceBar,true,false,300);Skip->TickAt(true,300.8);TestTrue(TEXT("Confirmed hold starts the fade"),Skip->Phase==Phase::FadeToBlack);
    Skip->HandleKeyAt(EKeys::SpaceBar,false,false,300.9);Skip->TickAt(true,301.05);TestTrue(TEXT("Fade reaches half black after .25 seconds"),FMath::IsNearlyEqual(Skip->BlackOpacity,.5f,.001f));TestEqual(TEXT("Playback stays normal before full black"),Player->GetCurrentTime().Time,StartTime);
    Skip->TickAt(true,301.3);TestTrue(TEXT("Acceleration starts only at full black after .5 seconds"),Skip->Phase==Phase::FastForward && Skip->BlackOpacity==1);
    Skip->TickAt(true,301.35);const double ExtraFrames=(Player->GetCurrentTime().Time-StartTime).AsDecimal()+Skip->FrameCredits.FindRef(Player);
    TestTrue(TEXT("Adds 19x frames to normal 1x playback for a 20x total time budget"),FMath::IsNearlyEqual(ExtraFrames,28.5,.001));TestEqual(TEXT("Authored play rate is preserved"),Player->GetPlayRate(),1.f);
    double Now=301.35;for(int I=0;I<80 && Skip->Phase==Phase::FastForward;++I){Now+=.05;Skip->TickAt(true,Now);}
    TestFalse(TEXT("Fast-forward reaches normal sequence completion"),Player->IsPlaying());TestEqual(TEXT("OnFinished is emitted once"),UVTGSequenceRegressionDirector::FinishCount,1);TestTrue(TEXT("Completion reveals the scene"),Skip->Phase==Phase::Reveal);
    Skip->TickAt(true,Now+.125);TestTrue(TEXT("Completion fade reveals gradually"),FMath::IsNearlyEqual(Skip->BlackOpacity,.5f,.001f));Skip->TickAt(true,Now+.26);TestTrue(TEXT("Completion leaves no prompt or black layer"),Skip->Phase==Phase::Idle && !Skip->bHintVisible && Skip->BlackOpacity==0);
    Player->Play();Settings->bAllowSkip=false;TestFalse(TEXT("No Skip sequences do not consume Space"),Skip->HandleKeyAt(EKeys::SpaceBar,true,false,400));TestFalse(TEXT("No Skip sequences do not advertise skipping"),Skip->bHintVisible);Settings->bAllowSkip=true;
    Actor->Tags.Add(TEXT("VTG.NoSkip"));Skip->NotifyActivityAt(EKeys::E,401);TestFalse(TEXT("Actor NoSkip tag also suppresses the prompt"),Skip->bHintVisible);Actor->Tags.Remove(TEXT("VTG.NoSkip"));
    Actor->PlaybackSettings.LoopCount.Value=-1;Skip->NotifyActivityAt(EKeys::E,402);TestFalse(TEXT("Looping background sequences never advertise skipping"),Skip->bHintVisible);Actor->PlaybackSettings.LoopCount.Value=0;
    auto BeginFast=[&](double Time){Skip->BeforeTravel({});Player->Stop();Player->Play();Skip->HandleKeyAt(EKeys::SpaceBar,true,false,Time);Skip->TickAt(true,Time+.8);Skip->TickAt(true,Time+1.3);};
    BeginFast(500);Skip->TickAt(false,501.31);TestTrue(TEXT("Focus loss immediately removes black and resets consumed input"),Skip->BlackOpacity==0 && !Skip->bConsumeRelease && Skip->Phase==Phase::Idle);
    BeginFast(600);Player->Pause();Skip->TickAt(true,601.31);TestTrue(TEXT("An authored pause interrupts fast-forward"),Skip->Phase==Phase::Reveal && Player->IsPaused());Skip->TickAt(true,601.6);Player->Play();
    auto* Owner=World->SpawnActor<AActor>();auto* Narrative=NewObject<UNarrativeComponent>(Owner);Owner->AddInstanceComponent(Narrative);Narrative->RegisterComponent();BeginFast(700);Narrative->CurrentDialogue=NewObject<UVTGReturnRegressionDialogue>(Narrative);Skip->TickAt(true,701.31);
    TestTrue(TEXT("Dialogue immediately clears the black layer"),Skip->BlackOpacity==0 && Skip->Phase==Phase::Idle);TestFalse(TEXT("Dialogue Space is left to the existing dialogue controls"),Skip->HandleKeyAt(EKeys::SpaceBar,true,false,702));TestFalse(TEXT("Dialogue never gets a cinematic skip hint"),Skip->bHintVisible);Narrative->CurrentDialogue=nullptr;
    // Reentrant travel cancellation while an event track is being evaluated must not index reset arrays.
    auto* Director=FindFProperty<FClassProperty>(ULevelSequence::StaticClass(),TEXT("DirectorClass"));Director->SetObjectPropertyValue_InContainer(Sequence,UVTGSequenceRegressionDirector::StaticClass());
    auto* Track=Scene->AddTrack<UMovieSceneEventTrack>();auto* Trigger=NewObject<UMovieSceneEventTriggerSection>(Track);Trigger->SetRange(TRange<FFrameNumber>(FFrameNumber(0),FFrameNumber(300)));Track->AddSection(*Trigger);FMovieSceneEvent Event;Event.Ptrs.Function=UVTGSequenceRegressionDirector::StaticClass()->FindFunctionByName(TEXT("Trigger"));Trigger->EventChannel.GetData().AddKey(FFrameNumber(5),Event);
    UVTGSequenceRegressionDirector::TriggerCallback.BindLambda([Skip](){Skip->BeforeTravel({});});BeginFast(800);Skip->TickAt(true,801.35);UVTGSequenceRegressionDirector::TriggerCallback.Unbind();TestTrue(TEXT("Travel during event evaluation clears black and skip input safely"),Skip->Phase==Phase::Idle && !Skip->bConsumeRelease && Skip->BlackOpacity==0);
    Player->Stop();GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GI->RemoveFromRoot();
    return true;
}
bool VTGRunSequenceSkipChecks()
{
    // Run these synchronous tests directly; the editor-wide automation log router recurses in commandlets.
    class FDirectCinematic : public FVTGCinematicRegression
    {public:FDirectCinematic():FVTGCinematicRegression(TEXT("VTGDirectCinematic")){}using FVTGCinematicRegression::RunTest;};
    class FDirectPrompt : public FVTGSequenceSkipPromptRegression
    {public:FDirectPrompt():FVTGSequenceSkipPromptRegression(TEXT("VTGDirectPrompt")){}using FVTGSequenceSkipPromptRegression::RunTest;};
    FDirectCinematic Cinematic;FDirectPrompt Prompt;
    bool Passed=true;
    for(auto* Test:{static_cast<FAutomationTestBase*>(&Cinematic),static_cast<FAutomationTestBase*>(&Prompt)})
    {
        const bool Ran=Test==&Cinematic?Cinematic.RunTest({}):Prompt.RunTest({});
        FAutomationTestExecutionInfo Info;Test->GetExecutionInfo(Info);
        const bool Success=Ran && !Test->HasAnyErrors();Passed&=Success;
        UE_LOG(LogTemp,Display,TEXT("CUTSCENE CHECK %s: %s"),*Test->GetTestFullName(),Success?TEXT("PASS"):TEXT("FAIL"));
        for(const auto& Entry:Info.GetEntries())UE_LOG(LogTemp,Display,TEXT("CHECK RESULT: %s"),*Entry.ToString());
    }
    return Passed;
}
#endif
