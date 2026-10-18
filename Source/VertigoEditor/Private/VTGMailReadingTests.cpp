#include "VTGMailReadingComponent.h"
#include "VTGNoteInputTestFixture.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "DialogueSM.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGBoundNodeFinishTest, "Vertigo.Dialogue.BoundNodeFinishOptIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGBoundNodeFinishTest::RunTest(const FString&)
{
    auto* Dialogue = NewObject<UVTGNoteInputTestDialogue>();
    auto* Node = NewObject<UDialogueNode_NPC>(Dialogue);
    Node->OnPlayNodeFuncName = TEXT("RecordNodeDispatchForTest");
    Dialogue->DispatchNodeForTest(nullptr, Node, true);
    Dialogue->DispatchNodeForTest(nullptr, Node, false);
    TestEqual(TEXT("Existing nodes keep one start callback"), Dialogue->NodeStartCount, 1);
    TestEqual(TEXT("Existing nodes do not gain an unexpected finish callback"), Dialogue->NodeFinishCount, 0);
    Node->bCallBoundEventOnFinish = true;
    Dialogue->DispatchNodeForTest(nullptr, Node, true);
    Dialogue->DispatchNodeForTest(nullptr, Node, false);
    TestEqual(TEXT("Finish-only opt-in does not also fire at start"), Dialogue->NodeStartCount, 1);
    TestEqual(TEXT("Finish-only opt-in dispatches the real finish callback"), Dialogue->NodeFinishCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGMailReadingTest, "Vertigo.Dialogue.MailReadingHandoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGMailReadingTest::RunTest(const FString&)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Reader = World->SpawnActor<AVTGMailReadingTestActor>();
    auto* Mail = UVTGMailReadingComponent::GetOrCreate(Reader);
    auto* Source = NewObject<USceneComponent>(); auto* Note = NewObject<USceneComponent>(); auto* OldNote = NewObject<USceneComponent>();
    const FName Cleanup(TEXT("FinishMailForTest"));
    TestFalse(TEXT("Ordinary dialogue ending does not defer mail cleanup"), UVTGMailReadingComponent::DeferMailReadingFinish(Reader, Cleanup));
    Mail->SetHold(Source, true); // Before the dialogue exits; Note widget does not exist yet.
    TestTrue(TEXT("Dialogue -> Note opening gap retains the reading pose"), UVTGMailReadingComponent::DeferMailReadingFinish(Reader, Cleanup));
    TestTrue(TEXT("Repeated dialogue finish queues a single cleanup"), UVTGMailReadingComponent::DeferMailReadingFinish(Reader, Cleanup));
    Mail->NoteOpened(Note);
    Mail->NoteClosed(OldNote);
    TestEqual(TEXT("An unrelated cached Note cannot finish this mail"), Reader->CleanupCount, 0);
    Mail->NoteClosed(Note);
    TestEqual(TEXT("Logical Note close resumes mail cleanup once"), Reader->CleanupCount, 1);
    Mail->NoteClosed(Note); Mail->SetHold(Source, false);
    TestEqual(TEXT("Widget destruction and story callback do not repeat cleanup"), Reader->CleanupCount, 1);
    TestFalse(TEXT("After Note close, future ordinary mail is no longer held"), UVTGMailReadingComponent::DeferMailReadingFinish(Reader, Cleanup));
    Mail->SetHold(Source, true);
    UVTGMailReadingComponent::DeferMailReadingFinish(Reader, Cleanup);
    Mail->SetHold(Source, false);
    TestEqual(TEXT("Story close callback also releases a hold without a widget"), Reader->CleanupCount, 2);
    Mail->SetHold(Source, true);
    Mail->NoteOpened(Note); Mail->NoteClosed(Note);
    TestFalse(TEXT("A Note closed before dialogue end leaves no stale hold"), UVTGMailReadingComponent::DeferMailReadingFinish(Reader, Cleanup));
    World->DestroyWorld(false);
    return true;
}
#endif
