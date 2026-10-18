#include "VTGNoteInputTestFixture.h"
#include "NarrativeUIInputComponent.h"
#include "UI/VTGNoteInputWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Tests/AutomationEditorCommon.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGNoteInputTest, "Vertigo.Dialogue.NoteInputOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGNoteInputTest::RunTest(const FString&)
{
    auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Controller = World->SpawnActor<AVTGPlayerController>();
    auto* Input = UNarrativeUIInputComponent::GetOrCreate(Controller);
    if (!TestNotNull(TEXT("Local player input owner"), Input)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    UObject* Note = NewObject<UVTGNoteInputTestDialogue>(Controller);
    UObject* OtherNote = NewObject<UVTGNoteInputTestDialogue>(Controller);
    auto* Dialogue = NewObject<UVTGNoteInputTestDialogue>(Controller);

    // Reproduce the bug using the real dialogue begin/end implementation.
    Dialogue->Start(Controller);
    Input->Acquire(Note, nullptr, 100);
    Dialogue->Finish();
    TestEqual(TEXT("Dialogue ending leaves Note in charge"), Input->GetActiveOwner(), Note);
    TestTrue(TEXT("Note keeps mouse cursor"), Controller->bShowMouseCursor);
    TestTrue(TEXT("Note freezes movement and look"), Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored());
    Input->Acquire(Note, nullptr, 100);
    Input->Release(Note);
    Input->Release(Note);
    TestFalse(TEXT("Repeated acquire/release does not leave movement locked"), Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());
    TestFalse(TEXT("Last modal close restores game cursor"), Controller->bShowMouseCursor);

    // A new dialogue may start in the Note's close callback.
    Input->Acquire(Note, nullptr, 100);
    auto* Followup = NewObject<UVTGNoteInputTestDialogue>(Controller);
    Followup->Start(Controller);
    TestEqual(TEXT("Opening dialogue cannot displace an open Note"), Input->GetActiveOwner(), Note);
    Input->Release(Note);
    Controller->SetInputMode(FInputModeGameOnly()); // InventorySystemX close callback.
    Controller->SetShowMouseCursor(false);
    Input->Refresh();
    TestEqual(TEXT("Note close returns ownership to follow-up dialogue"), Input->GetActiveOwner(), static_cast<UObject*>(Followup));
    TestTrue(TEXT("Follow-up dialogue restores UI cursor after legacy callback"), Controller->bShowMouseCursor);
    Followup->Finish();
    TestFalse(TEXT("Follow-up dialogue releases the final lock"), Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());

    // Two modals and an independent cinematic lock must remain balanced.
    Controller->SetIgnoreMoveInput(true);
    Controller->SetIgnoreLookInput(true);
    Input->Acquire(Note, nullptr, 100);
    Input->Acquire(OtherNote, nullptr, 100);
    Input->Release(Note);
    TestEqual(TEXT("Removing an older modal preserves the newer modal"), Input->GetActiveOwner(), OtherNote);
    Input->Release(OtherNote);
    TestTrue(TEXT("Independent cinematic lock is preserved"), Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored());
    Controller->SetIgnoreMoveInput(false);
    Controller->SetIgnoreLookInput(false);

    auto* NoteClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Inventory/InventoryXBased/UMG/WB_NoteMainWidget.WB_NoteMainWidget_C"));
    TestTrue(TEXT("Actual project Note root uses native input ownership"), NoteClass && NoteClass->IsChildOf(UVTGNoteInputWidget::StaticClass()));
    auto* ControllerClass = LoadClass<APlayerController>(nullptr, TEXT("/Game/ThirdPerson/PC_Game_VTG.PC_Game_VTG_C"));
    TestTrue(TEXT("Actual project controller protects modal UI input"), ControllerClass && ControllerClass->IsChildOf(AVTGPlayerController::StaticClass()));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGNoteInputLifecycleTest, "Vertigo.Dialogue.NoteInputLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FVTGNoteInputLifecycleScenario : public IAutomationLatentCommand
{
public:
    explicit FVTGNoteInputLifecycleScenario(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) World = Context.World();
        if (!World || !World->GetFirstPlayerController())
        {
            if (!Deadline) Deadline = FPlatformTime::Seconds() + 15.;
            if (FPlatformTime::Seconds() < Deadline) return false;
            Test->AddError(TEXT("Note lifecycle PIE did not start"));
            return true;
        }
        auto* PC = World->GetFirstPlayerController();
        auto* Input = UNarrativeUIInputComponent::GetOrCreate(PC);
        if (!Input) { Test->AddError(TEXT("No local UI input component")); return true; }
        if (Phase == 0)
        {
            Dialogue.Reset(NewObject<UVTGNoteInputTestDialogue>(PC));
            Dialogue->Start(PC);
            Note = CreateWidget<UVTGNoteInputTestWidget>(PC);
            if (!Note.IsValid()) { Test->AddError(TEXT("Could not create Note widget")); return true; }
            Note->AddToViewport(100);
        }
        else if (Phase == 1)
        {
            Test->TestEqual(TEXT("Viewport Note acquires input automatically"), Input->GetActiveOwner(), static_cast<UObject*>(Note.Get()));
            Test->TestTrue(TEXT("Note receives UI keyboard focus"), Note->HasUserFocus(PC));
            Dialogue->Finish();
            WaitUntil = FPlatformTime::Seconds() + .85;
        }
        else if (Phase == 2)
        {
            if (FPlatformTime::Seconds() < WaitUntil) return false;
            // Match the real BP_Player_Sa mail-camera cleanup, which runs .8 seconds after mail ends.
            PC->SetInputMode(FInputModeGameOnly());
            PC->SetShowMouseCursor(false);
            Test->TestTrue(TEXT("Late mail restore cannot re-enable viewport gameplay input"), PC->GetLocalPlayer()->ViewportClient->IgnoreInput());
            Test->TestTrue(TEXT("Late cursor hide cannot hide the open Note's cursor"), PC->ShouldShowMouseCursor());
            PC->SetInputMode(FInputModeGameAndUI());
            Test->TestTrue(TEXT("Legacy controller GameAndUI also stays UI-only"), PC->GetLocalPlayer()->ViewportClient->IgnoreInput());
            Test->TestEqual(TEXT("Real dialogue end cannot steal Note input"), Input->GetActiveOwner(), static_cast<UObject*>(Note.Get()));
            Test->TestTrue(TEXT("Open Note keeps cursor and freezes look"), PC->ShouldShowMouseCursor() && PC->IsLookInputIgnored());
            Note->SetNoteInputActive(false);
            Note->SetVisibility(ESlateVisibility::Collapsed);
        }
        else if (Phase == 3)
        {
            Test->TestTrue(TEXT("Closed Note root is deliberately retained in viewport"), Note->IsInViewport());
            Test->TestFalse(TEXT("Logical close releases Note before Destruct"), Input->HasOwners());
            Test->TestFalse(TEXT("Closing last UI restores gameplay"), PC->bShowMouseCursor || PC->IsLookInputIgnored() || PC->IsMoveInputIgnored());
            Test->TestFalse(TEXT("Logical close restores actual viewport gameplay input"), PC->GetLocalPlayer()->ViewportClient->IgnoreInput());
            Note->SetVisibility(ESlateVisibility::Visible);
            Note->SetNoteInputActive(true);
            Test->TestEqual(TEXT("Cached Note can reacquire input without Construct"), Input->GetActiveOwner(), static_cast<UObject*>(Note.Get()));
            Dialogue.Reset(NewObject<UVTGNoteInputTestDialogue>(PC));
            Dialogue->Start(PC);
        }
        else if (Phase == 4)
        {
            Note->SetNoteInputActive(false);
            Note->SetVisibility(ESlateVisibility::Collapsed);
            PC->SetInputMode(FInputModeGameOnly());
            PC->SetShowMouseCursor(false); // Reproduce InventorySystemX's trailing close callback.
        }
        else if (Phase == 5)
        {
            Test->TestEqual(TEXT("Note teardown preserves follow-up dialogue"), Input->GetActiveOwner(), static_cast<UObject*>(Dialogue.Get()));
            Test->TestTrue(TEXT("Controller handoff preserves dialogue UI"), PC->ShouldShowMouseCursor() && PC->IsLookInputIgnored());
            Test->TestTrue(TEXT("Viewport remains UI-only after late Note close callback"), PC->GetLocalPlayer()->ViewportClient->IgnoreInput());
            Dialogue->Finish();
            Test->TestFalse(TEXT("Final dialogue releases player lock"), PC->IsLookInputIgnored() || PC->IsMoveInputIgnored());
            Test->TestFalse(TEXT("Final dialogue restores actual gameplay input"), PC->GetLocalPlayer()->ViewportClient->IgnoreInput());
            Note->RemoveFromParent();
            return true;
        }
        ++Phase;
        return false;
    }
private:
    FAutomationTestBase* Test;
    TStrongObjectPtr<UVTGNoteInputTestDialogue> Dialogue;
    TWeakObjectPtr<UVTGNoteInputTestWidget> Note;
    double Deadline = 0;
    double WaitUntil = 0;
    int32 Phase = 0;
};

bool FVTGNoteInputLifecycleTest::RunTest(const FString&)
{
    auto* World = FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode = AVTGNoteInputTestGameMode::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVTGNoteInputLifecycleScenario>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
