#include "VTGMailReadingComponent.h"
#include "VTGNoteInputTestFixture.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "DialogueSM.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGNoteMailCameraSessionTest, "Vertigo.Dialogue.NoteMailCameraSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGNoteMailCameraSessionTest::RunTest(const FString&)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Reader = World->SpawnActor<AVTGMailReadingTestActor>();
    auto* Mail = UVTGMailReadingComponent::GetOrCreate(Reader);
    const FName Step(TEXT("FinishMailForTest"));
    auto Advance = [&]() { ++GFrameCounter; World->GetTimerManager().Tick(0.f); ++GFrameCounter; World->GetTimerManager().Tick(.3f); };
    TestFalse(TEXT("Ordinary mail retains its original Blueprint delay"), UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step));
    UVTGMailReadingComponent::NotifyNoteMailDialogueStarted(Reader);
    TestFalse(TEXT("Unrelated dialogue does not alter reading state"),Reader->IsReadingMail);
    auto* Source = NewObject<USceneComponent>();
    Mail->SetHold(Source,true);
    TestFalse(TEXT("A Note alone does not arm the mail camera guard"), UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step));
    TestTrue(TEXT("Only actual deferred mail cleanup arms protection"),UVTGMailReadingComponent::DeferMailReadingFinish(Reader,Step));
    Mail->SetHold(Source,false);
    const int32 Before = Reader->CleanupCount;
    TestTrue(TEXT("Deferred Note flow schedules a protected camera step"),UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step));
    UVTGMailReadingComponent::NotifyNoteMailDialogueStarted(Reader);
    TestTrue(TEXT("Direct Note -> mail dialogue retains its own finish cleanup"),Reader->IsReadingMail);
    Advance();
    TestEqual(TEXT("Older step cannot change a newer same-actor mail camera"),Reader->CleanupCount,Before);
    TestTrue(TEXT("Newer session can perform its own cleanup"),UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step));
    Advance();
    TestEqual(TEXT("Current session continuation executes once"),Reader->CleanupCount,Before+1);
    UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step,true);
    UVTGMailReadingComponent::NotifyMailReadingStarted(Reader);
    Advance();
    TestEqual(TEXT("Old final input restore cannot run into another reading"),Reader->CleanupCount,Before+1);
    UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step,true);
    Advance();
    TestEqual(TEXT("Final current cleanup runs normally"),Reader->CleanupCount,Before+2);
    TestFalse(TEXT("Following ordinary mail returns to original scheduling"),UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(Reader,.2f,Step));
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGNoteMailCameraReturnTest, "Vertigo.Dialogue.NoteMailCameraReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGNoteMailCameraReturnTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Reader = World->SpawnActor<APawn>();
    auto* PC = World->SpawnActor<APlayerController>();
    PC->Possess(Reader);
    if (!PC->PlayerCameraManager)
    {
        FActorSpawnParameters Params; Params.Owner=PC;
        PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>(APlayerCameraManager::StaticClass(),Params);
        PC->PlayerCameraManager->InitializeFor(PC);
    }
    auto* Fixed = World->SpawnActor<AActor>();
    auto* Lock = World->SpawnActor<AActor>();
    auto* Later = World->SpawnActor<AActor>();
    auto* Follow = NewObject<UCameraComponent>(Reader); Reader->AddInstanceComponent(Follow); Follow->RegisterComponent();
    auto* Terminal = NewObject<UCameraComponent>(Reader); Reader->AddInstanceComponent(Terminal); Terminal->RegisterComponent();
    auto* Hold = NewObject<USceneComponent>();
    auto* Mail = UVTGMailReadingComponent::GetOrCreate(Reader);
    for (auto* Original : {Fixed,Lock,static_cast<AActor*>(Reader)})
    {
        Follow->SetActive(true); Terminal->SetActive(false);
        PC->SetViewTarget(Original);
        UVTGMailReadingComponent::CaptureMailCameraReturn(Reader);
        TestFalse(TEXT("Ordinary mail keeps legacy return"),UVTGMailReadingComponent::RestoreNoteMailCamera(Reader));
        PC->SetViewTarget(Reader); Follow->SetActive(false); Terminal->SetActive(true);
        Mail->SetHold(Hold,true);
        UVTGMailReadingComponent::DeferMailReadingFinish(Reader,TEXT("Unused"));
        // The second reading must not replace the original with the terminal view.
        UVTGMailReadingComponent::CaptureMailCameraReturn(Reader);
        PC->SetViewTargetWithBlend(Later,1.f);
        UVTGMailReadingComponent::RestoreNoteMailCamera(Reader);
        TestTrue(TEXT("A pending scene blend is not overwritten"),PC->PlayerCameraManager->PendingViewTarget.Target==Later);
        PC->SetViewTarget(Reader);
        PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
        TestTrue(TEXT("Note mail return handled"),UVTGMailReadingComponent::RestoreNoteMailCamera(Reader));
        TestTrue(TEXT("Camera restoration preserves independent input locks"),PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());
        PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
        auto* Manager=PC->PlayerCameraManager.Get();
        TestTrue(TEXT("Returns to original fixed / lock / third-person target"),Manager->GetViewTarget()==Original || Manager->PendingViewTarget.Target==Original);
        if(Original==Reader) TestTrue(TEXT("Third person restores Follow instead of Terminal"),Follow->IsActive() && !Terminal->IsActive());
        PC->SetViewTarget(Later);
        UVTGMailReadingComponent::RestoreNoteMailCamera(Reader);
        TestTrue(TEXT("A later scene camera is never reclaimed"),PC->GetViewTarget()==Later && !Manager->PendingViewTarget.Target);
        // Independent cases use independent component state.
        Mail->DestroyComponent(); Mail=UVTGMailReadingComponent::GetOrCreate(Reader);
    }
    World->DestroyWorld(false);
    return true;
}
#endif
