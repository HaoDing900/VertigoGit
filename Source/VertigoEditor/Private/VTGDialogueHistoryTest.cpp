#include "DialogueHistoryEvents.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Components/ScaleBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Input/CommonBoundActionBar.h"
#include "VTGDialoguePlaybackTest.h"
#include "Input/UIActionBinding.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"
#include "PlayInEditorDataTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "NarrativeComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/VTGDialogueHistorySubsystem.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGDialogueHistoryTest, "Vertigo.Dialogue.History",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FVTGHistoryScenario : public IAutomationLatentCommand
{
  public:
    explicit FVTGHistoryScenario(FAutomationTestBase *InTest) : Test(InTest)
    {
    }
    bool Update() override
    {
        UWorld *World = nullptr;
        for (const FWorldContext &Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE)
                World = Context.World();
        if (!World)
        {
            if (Deadline == 0.0)
                Deadline = FPlatformTime::Seconds() + 10;
            if (FPlatformTime::Seconds() < Deadline)
                return false;
            Test->AddError(TEXT("PIE did not start."));
            return true;
        }
        auto *System = World->GetGameInstance()->GetSubsystem<UVTGDialogueHistorySubsystem>();
        if (!System)
        {
            Test->AddError(TEXT("History subsystem not created."));
            return true;
        }
        if (Phase == 0)
        {
            Test->TestFalse(TEXT("Esc outside dialogue remains available"), System->HandleEscape());
            Test->TestFalse(TEXT("Wheel outside dialogue remains available"), System->HandleWheel(1));
            auto *PC = World->GetFirstPlayerController();
            Component = NewObject<UNarrativeComponent>(PC);
            PC->AddInstanceComponent(Component.Get());
            Component->RegisterComponent();
            auto *Dialogue = NewObject<UDialogue>(Component.Get());
            Dialogue->OwningComp = Component.Get();
            Dialogue->bCanBeExited = true;
            Component->CurrentDialogue = Dialogue;
            OnNarrativeLinePresented().Broadcast(Component.Get(), FText::FromString(TEXT("酒保")),
                                                 FText::FromString(TEXT("晚上好。你想喝点什么？")));
            OnNarrativeLinePresented().Broadcast(Component.Get(), FText::FromString(TEXT("Sa")),
                                                 FText::FromString(TEXT("我在找一个人。")));
            OnNarrativeLinePresented().Broadcast(Component.Get(), FText::FromString(TEXT("酒保")),
                                                 FText::FromString(TEXT("先坐下吧。<Default>慢慢说。</>")));
            Test->TestEqual(TEXT("Presented NPC and player lines are recorded"), System->Entries.Num(), 3);
            Test->TestEqual(TEXT("Speaker is preserved"), System->Entries[1].Speaker.ToString(), FString(TEXT("Sa")));
            Test->TestTrue(TEXT("Esc during dialogue consumed"), System->HandleEscape());
            Test->TestTrue(TEXT("Esc did not exit dialogue"), Component->IsInDialogue());
            Test->TestTrue(TEXT("Esc opens existing pause menu"), UGameplayStatics::IsGamePaused(World));
            System->HandleEscape();
            Test->TestFalse(TEXT("Esc resumes without ending dialogue"), UGameplayStatics::IsGamePaused(World));
            System->ToggleAutoAdvance();
            Test->TestFalse(TEXT("Manual mode reaches active dialogue"), Dialogue->IsAutoAdvanceEnabled());
            System->ToggleAutoAdvance();
            Test->TestTrue(TEXT("Automatic mode restored"), Dialogue->IsAutoAdvanceEnabled());
            Playback.Reset(NewObject<UVTGDialoguePlaybackTest>(Component.Get()));
            Playback->Prepare(Component.Get(), true);
            UGameplayStatics::SetGamePaused(World, true);
            Test->TestFalse(TEXT("Pause blocks the existing skip route for all input devices"), Playback->CanSkipCurrentLine());
            UGameplayStatics::SetGamePaused(World, false);
            Test->TestTrue(TEXT("Resuming restores the existing skip route"), Playback->CanSkipCurrentLine());
            Playback->SetAutoAdvance(false);
            Playback->NaturalFinish();
            Test->TestEqual(TEXT("Manual mode waits after natural line end"), Playback->FinishedLines, 0);
            Test->TestTrue(TEXT("Manual line is waiting"), Playback->IsWaitingForManualAdvance());
            Playback->ManualFinish();
            Test->TestEqual(TEXT("Existing skip completion advances manual line once"), Playback->FinishedLines, 1);
            Playback->Prepare(Component.Get(), false);
            Playback->NaturalFinish();
            Test->TestEqual(TEXT("Unskippable story lines retain natural completion"), Playback->FinishedLines, 2);
            Playback->Prepare(Component.Get(), true, true);
            Playback->NaturalFinish();
            Test->TestEqual(TEXT("Empty routing nodes never wait for input"), Playback->FinishedLines, 3);
            Playback->Prepare(Component.Get(), true);
            Playback->NaturalFinish();
            Playback->SetAutoAdvance(true);
            Test->TestEqual(TEXT("Resuming automatic playback is deferred safely"), Playback->FinishedLines, 3);

            auto* Fast = NewObject<UVTGDialoguePlaybackTest>(Component.Get());
            Fast->Prepare(Component.Get(), true);
            Component->CurrentDialogue = Fast;
            const double HoldStart = FPlatformTime::Seconds();
            System->SetSkipHeld(true);
            System->TickSkip(HoldStart + .20);
            Test->TestEqual(TEXT("Short press does not fast-forward"), Fast->FinishedLines, 0);
            System->TickSkip(HoldStart + .5);
            System->TickSkip(HoldStart + .55);
            Test->TestEqual(TEXT("Hold starts one line and rate-limits repeat"), Fast->FinishedLines, 1);
            System->TickSkip(HoldStart + .65);
            Test->TestEqual(TEXT("Held skip repeatedly uses real narrative advance"), Fast->FinishedLines, 2);
            System->HandleSkipKey(EKeys::LeftControl, false);
            System->TickSkip(HoldStart + 2);
            Test->TestEqual(TEXT("Releasing stops repetition immediately"), Fast->FinishedLines, 2);
            System->SetSkipHeld(true);
            Fast->WaitForChoice();
            System->TickSkip(HoldStart + 3);
            Test->TestFalse(TEXT("Skip cancels at player choice"), System->IsSkipHeld());
            Test->TestFalse(TEXT("Continue also cannot replay the last line while choices are visible"), Fast->CanSkipCurrentLine());
            Fast->Prepare(Component.Get(), false);
            System->SetSkipHeld(true);
            Test->TestFalse(TEXT("Cannot begin fast-forward on unskippable story line"), System->IsSkipHeld());
            Fast->Prepare(Component.Get(), true);
            System->SetSkipHeld(true);
            System->ShowHistory();
            Test->TestFalse(TEXT("Opening history cancels hold"), System->IsSkipHeld());
            System->HideHistory();
            System->SetSkipHeld(true);
            UGameplayStatics::SetGamePaused(World, true);
            System->TickSkip(HoldStart + 4);
            Test->TestFalse(TEXT("Pause cancels fast-forward"), System->IsSkipHeld());
            UGameplayStatics::SetGamePaused(World, false);
            System->SetSkipHeld(true);
            Component->CurrentDialogue = Dialogue;
            System->TickSkip(HoldStart + 5);
            Test->TestFalse(TEXT("Held skip never carries into a replacement dialogue"), System->IsSkipHeld());

            auto* MenuClass = LoadClass<UNarrativeActivatableWidget>(nullptr, TEXT("/Game/Narrative/NarrativeUI/Widgets/W_NarrativeMenu_Dialogue.W_NarrativeMenu_Dialogue_C"));
            Menu = CreateWidget<UNarrativeActivatableWidget>(PC, MenuClass);
            if (Menu.IsValid())
            {
                if (auto* Prop = FindFProperty<FObjectPropertyBase>(Menu->GetClass(), TEXT("OwnerNarrativeComponent")))
                    Prop->SetObjectPropertyValue_InContainer(Menu.Get(), Component.Get());
                Menu->AddToViewport();
                Menu->ActivateWidget();
                System->TickControls();
                int32 Added = 0;
                for (const auto Handle : Menu->GetActionBindings())
                    if (auto Binding = FUIActionBinding::FindBinding(Handle))
                        {
                            Test->AddInfo(Binding->ToDebugString());
                            const FName Row = Binding->LegacyActionTableRow.RowName;
                            if (Row == TEXT("ToggleAuto") || Row == TEXT("History") || Row == TEXT("SkipHold")) ++Added;
                            if (Row == TEXT("Continue")) Test->TestEqual(TEXT("Space prompt renamed Continue"), Binding->ActionDisplayName.ToString(), FString(TEXT("Continue")));
                            if (Row == TEXT("ToggleAuto"))
                            {
                                const bool Before = System->IsAutoAdvanceEnabled();
                                Binding->OnExecuteAction.ExecuteIfBound();
                                Test->TestTrue(TEXT("Auto prompt callback actually toggles mode"), Before != System->IsAutoAdvanceEnabled());
                                Binding->OnExecuteAction.ExecuteIfBound();
                            }
                        }
                Test->TestEqual(TEXT("Auto, history and held skip use actual dialogue menu CommonUI bindings"), Added, 3);
            }
            else Test->AddError(TEXT("Actual dialogue menu could not be created"));
            Test->TestTrue(TEXT("Scroll up opens history"), System->HandleWheel(1));
            Test->TestTrue(TEXT("History panel exists"), System->IsHistoryOpen());
            Deadline = FPlatformTime::Seconds() + 1.0;
            Phase = 1;
            return false;
        }
        if (FPlatformTime::Seconds() < Deadline)
            return false;
        if (Phase == 1)
        {
            Test->TestEqual(TEXT("Switching back to automatic completes a waiting line"), Playback->FinishedLines, 4);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/DialogueHistory.png"),
                                                  true, false);
            Phase = 2;
            Deadline = FPlatformTime::Seconds() + .5;
            return false;
        }
        if (Phase == 2)
        {
            Test->TestTrue(TEXT("Esc closes log"), System->HandleEscape());
            auto* PC = World->GetFirstPlayerController();
            auto* OverlayClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C"));
            auto* HUDClass = LoadClass<UUserWidget>(nullptr, TEXT("/NarrativeCommonUI/Widgets/WBP_NarrativeHUD.WBP_NarrativeHUD_C"));
            VisualOverlay = CreateWidget<UUserWidget>(PC, OverlayClass);
            VisualHUD = CreateWidget<UUserWidget>(PC, HUDClass);
            // Render the artist's actual widget trees without executing unrelated story/tick bindings.
            VisualOverlay->SetDesignerFlags(EWidgetDesignFlags::Designing);

            VisualHUD->AddToViewport(21);
            // Match the real game's nested dialogue overlay. IsInViewport is
            // false here even though the parent HUD is mounted and visible.
            auto* NestedSlot = CastChecked<UOverlay>(VisualHUD->WidgetTree->RootWidget)->AddChildToOverlay(VisualOverlay.Get());
            NestedSlot->SetHorizontalAlignment(HAlign_Fill);
            NestedSlot->SetVerticalAlignment(VAlign_Fill);
            Test->TestFalse(TEXT("Nested dialogue overlay is not a top-level viewport widget"), VisualOverlay->IsInViewport());
            // Reproduce another HUD created during a scene transition. Both bars
            // subscribe to the same CommonUI actions, even with one menu.
            DuplicateHUD = CreateWidget<UUserWidget>(PC, HUDClass);
            DuplicateHUD->AddToViewport(22);
            FirstPromptBar = Cast<UCommonBoundActionBar>(VisualHUD->GetWidgetFromName(TEXT("CommonBoundActionBar")));
            SecondPromptBar = Cast<UCommonBoundActionBar>(DuplicateHUD->GetWidgetFromName(TEXT("CommonBoundActionBar")));
            auto* Root = Cast<UPanelWidget>(VisualOverlay->WidgetTree->RootWidget);
            for (UWidget* Child : Root->GetAllChildren()) Child->SetVisibility(ESlateVisibility::Collapsed);
            auto* Frame = VisualOverlay->GetWidgetFromName(TEXT("DialogueBox"));
            Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            auto* Slot = CastChecked<UCanvasPanelSlot>(Frame->Slot);
            Slot->SetAnchors(FAnchors(.10f, .45f, .90f, .94f));
            Slot->SetOffsets(FMargin(0));
            Slot->SetAlignment(FVector2D::ZeroVector);
            auto* Line = Cast<URichTextBlock>(VisualOverlay->GetWidgetFromName(TEXT("Line_Main")));
            Line->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            Line->SetText(FText::FromString(TEXT("我那时候还是个小屁孩。这里是对话正文，按键提示应当位于框体内部。")));
            auto* LineSlot = CastChecked<UCanvasPanelSlot>(Line->Slot);
            LineSlot->SetAnchors(FAnchors(.28f, .57f, .80f, .73f));
            LineSlot->SetOffsets(FMargin(0));
            ResizeGameWindow(World, FIntPoint(1280,720));
            Phase = 3;
            Deadline = FPlatformTime::Seconds() + 1;
            return false;
        }
        if (Phase == 3)
        {
            auto* Frame = VisualOverlay->GetWidgetFromName(TEXT("DialogueBox"));
            auto* Footer = VisualOverlay->GetWidgetFromName(TEXT("VTG_DialoguePromptFooter"));
            Test->TestNotNull(TEXT("Prompt footer exists inside actual dialogue widget"), Footer);
            if (Footer)
            {
                const FIntPoint ExpectedSizes[] = {{1280, 720}, {1920, 720}, {960, 960}, {800, 1000}};
                auto* SceneViewport = World->GetGameInstance()->GetGameViewportClient()->GetGameViewport();
                Test->TestEqual(TEXT("Actual game viewport resolution changed"), SceneViewport->GetSizeXY(), ExpectedSizes[AspectIndex]);
                const FVector2D RootSize = VisualOverlay->WidgetTree->RootWidget->GetCachedGeometry().GetLocalSize();
                Test->TestTrue(TEXT("Nested dialogue actually paints with a nonzero size"), RootSize.X > 100 && RootSize.Y > 100);
                Test->TestTrue(TEXT("Actual UMG root aspect ratio changed, not just the dialogue size"),
                    FMath::IsNearlyEqual(RootSize.X / RootSize.Y, double(ExpectedSizes[AspectIndex].X) / ExpectedSizes[AspectIndex].Y, .02));
                const auto& FG = Frame->GetCachedGeometry();
                const auto& PG = Footer->GetCachedGeometry();
                FVector2D Min = FG.AbsoluteToLocal(PG.LocalToAbsolute(FVector2D::ZeroVector));
                FVector2D Max = FG.AbsoluteToLocal(PG.LocalToAbsolute(PG.GetLocalSize()));
                Test->TestTrue(TEXT("All footer bounds remain inside dialogue frame after resize"),
                    Min.X >= 0 && Min.Y >= 0 && Max.X <= FG.GetLocalSize().X && Max.Y <= FG.GetLocalSize().Y);
                const FVector2D RelativeMin = Min / FVector2D(FG.GetLocalSize());
                const FVector2D RelativeMax = Max / FVector2D(FG.GetLocalSize());
                Test->TestTrue(TEXT("Footer keeps identical frame-relative position through DPI, aspect and render transforms"),
                    RelativeMin.Equals(FVector2D(.30,.64), .005) && RelativeMax.Equals(FVector2D(.95,.80), .005));
                Test->AddInfo(FString::Printf(TEXT("Viewport %dx%d; UMG %.1fx%.1f; footer normalized (%.3f,%.3f)-(%.3f,%.3f)"),
                    SceneViewport->GetSizeXY().X, SceneViewport->GetSizeXY().Y, RootSize.X, RootSize.Y,
                    RelativeMin.X, RelativeMin.Y, RelativeMax.X, RelativeMax.Y));
                auto* Bar = Cast<UCommonBoundActionBar>(CastChecked<UScaleBox>(Footer)->GetContent());
                Test->TestTrue(TEXT("Original live action bar is physically parented to dialogue footer"), Bar && Bar->GetParent() == Footer);
                auto* FirstBar = FirstPromptBar.Get();
                auto* SecondBar = SecondPromptBar.Get();
                Test->TestTrue(TEXT("Two HUDs display exactly one dialogue prompt bar"),
                    FirstBar && SecondBar && (int32(FirstBar->IsVisible()) + int32(SecondBar->IsVisible()) == 1));
                Test->TestTrue(TEXT("The only visible bar belongs to the dialogue footer"), Bar && Bar->IsVisible());
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Automation/DialogueAspect%d.png"), AspectIndex), true, false);
            Phase = 4;
            Deadline = FPlatformTime::Seconds() + .5;
            return false;
        }
        if (Phase == 4 && AspectIndex < 3)
        {
            ++AspectIndex;
            const FIntPoint Sizes[] = {{1280,720},{1920,720},{960,960},{800,1000}};
            ResizeGameWindow(World, Sizes[AspectIndex]);
            if (AspectIndex == 2)
            {
                auto* Frame = VisualOverlay->GetWidgetFromName(TEXT("DialogueBox"));
                FWidgetTransform Transform;
                Transform.Scale = FVector2D(.88,.78);
                Transform.Translation = FVector2D(20,-10);
                Frame->SetRenderTransformPivot(FVector2D(.31,.73));
                Frame->SetRenderTransform(Transform);
            }
            Phase = 3;
            Deadline = FPlatformTime::Seconds() + 1;
            return false;
        }
        // A live dialogue can temporarily hide its frame (cinematics/pauses).
        // It must not reveal the original screen-corner action bars.
        auto CheckHidden = [&]()
        {
            System->TickControls();
            Test->TestTrue(TEXT("No visible prompt bars without a visible dialogue frame"),
                FirstPromptBar.IsValid() && SecondPromptBar.IsValid() &&
                !FirstPromptBar->IsVisible() && !SecondPromptBar->IsVisible());
        };
        auto CheckShown = [&]()
        {
            System->TickControls();
            Test->TestTrue(TEXT("Showing the frame restores exactly one prompt bar"),
                int32(FirstPromptBar->IsVisible()) + int32(SecondPromptBar->IsVisible()) == 1);
            auto* Footer = VisualOverlay->GetWidgetFromName(TEXT("VTG_DialoguePromptFooter"));
            auto* ActiveBar = FirstPromptBar->IsVisible() ? FirstPromptBar.Get() : SecondPromptBar.Get();
            Test->TestTrue(TEXT("Restored prompts stay inside dialogue footer"), Footer && ActiveBar->GetParent() == Footer);
        };
        auto* Frame = VisualOverlay->GetWidgetFromName(TEXT("DialogueBox"));
        for (auto Visibility : {ESlateVisibility::Hidden, ESlateVisibility::Collapsed})
        {
            Frame->SetVisibility(Visibility);
            CheckHidden();
            Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            CheckShown();
        }
        Frame->SetRenderOpacity(0.f);
        CheckHidden();
        Frame->SetRenderOpacity(1.f);
        CheckShown();
        VisualOverlay->SetRenderOpacity(0.f);
        CheckHidden();
        VisualOverlay->SetRenderOpacity(1.f);
        CheckShown();
        VisualOverlay->SetVisibility(ESlateVisibility::Collapsed);
        CheckHidden();
        VisualOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        CheckShown();
        VisualOverlay->RemoveFromParent();
        CheckHidden();
        auto* NestedSlot = CastChecked<UOverlay>(VisualHUD->WidgetTree->RootWidget)->AddChildToOverlay(VisualOverlay.Get());
        NestedSlot->SetHorizontalAlignment(HAlign_Fill);
        NestedSlot->SetVerticalAlignment(VAlign_Fill);
        CheckShown();
        Test->TestFalse(TEXT("Panel removed"), System->IsHistoryOpen());
        Test->TestTrue(TEXT("Closing log preserves dialogue"), Component->IsInDialogue());
        System->HandleWheel(1);
        System->HandleWheel(-1);
        Test->TestFalse(TEXT("Scrolling down beyond newest returns to dialogue"), System->IsHistoryOpen());
        if (Menu.IsValid()) { Menu->DeactivateWidget(); Menu->RemoveFromParent(); }
        Component->ExitDialogue();
        Test->TestFalse(TEXT("Scripted dialogue completion still works"), Component->IsInDialogue());
        Test->TestEqual(TEXT("History persists after conversation"), System->Entries.Num(), 3);
        Test->TestFalse(TEXT("Esc released after conversation"), System->HandleEscape());
        System->TickControls();
        auto* Bar = Cast<UCommonBoundActionBar>(VisualHUD->GetWidgetFromName(TEXT("CommonBoundActionBar")));
        Test->TestTrue(TEXT("Dialogue completion restores original HUD action bar parent"), Bar && Bar->GetParent()->GetClass()->GetFName() == TEXT("Overlay"));
        auto* RestoredDuplicate = DuplicateHUD->GetWidgetFromName(TEXT("CommonBoundActionBar"));
        Test->TestTrue(TEXT("No screen-corner prompts remain after dialogue ends"),
            Bar && !Bar->IsVisible() && RestoredDuplicate && !RestoredDuplicate->IsVisible());
        DuplicateHUD->RemoveFromParent();
        VisualOverlay->RemoveFromParent();
        VisualHUD->RemoveFromParent();
        Component->DestroyComponent();
        Test->AddInfo(TEXT("History recording, wheel open/close, Esc protection, and normal exit PASS."));
        return true;
    }

  private:
    void ResizeGameWindow(UWorld* World, FIntPoint Size)
    {
        auto* Client = World->GetGameInstance()->GetGameViewportClient();
        if (auto Window = Client->GetWindow()) Window->Resize(FVector2D(Size));
        Client->GetGameViewport()->SetFixedViewportSize(Size.X, Size.Y);
    }
    FAutomationTestBase *Test;
    TStrongObjectPtr<UVTGDialoguePlaybackTest> Playback;
    TWeakObjectPtr<UNarrativeActivatableWidget> Menu;
    TWeakObjectPtr<UNarrativeComponent> Component;
    TWeakObjectPtr<UUserWidget> VisualOverlay, VisualHUD, DuplicateHUD;
    TWeakObjectPtr<UCommonBoundActionBar> FirstPromptBar, SecondPromptBar;
    int32 AspectIndex = 0;
    double Deadline = 0;
    int32 Phase = 0;
};
class FVTGRestorePlayWindowSettings : public IAutomationLatentCommand
{
public:
    FVTGRestorePlayWindowSettings()
    {
        const auto* Settings = GetDefault<ULevelEditorPlaySettings>();
        Width = Settings->NewWindowWidth;
        Height = Settings->NewWindowHeight;
        Position = Settings->NewWindowPosition;
        LastSize = Settings->LastSize;
        Positions = Settings->MultipleInstancePositions;
    }
    bool Update() override
    {
        // FEndPlayMapCommand only queues shutdown. Wait until UE has finished
        // persisting its last window before restoring the pre-test settings.
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) return false;
        auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->NewWindowWidth = Width;
        Settings->NewWindowHeight = Height;
        Settings->NewWindowPosition = Position;
        Settings->LastSize = LastSize;
        Settings->MultipleInstancePositions = Positions;
        Settings->PostEditChange();
        Settings->SaveConfig();
        return true;
    }
private:
    int32 Width, Height;
    FIntPoint Position, LastSize;
    TArray<FIntPoint> Positions;
};

bool FVTGDialogueHistoryTest::RunTest(const FString &)
{
    AddExpectedError(TEXT("Failed to load cursor"), EAutomationExpectedErrorFlags::Contains, 2);
    auto *World = FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    const auto RestoreSettings = MakeShared<FVTGRestorePlayWindowSettings>();
    FRequestPlaySessionParams Params;
    Params.bAllowOnlineSubsystem = false;
    // No destination editor viewport: use an independent PIE window that can really change aspect.
    GEditor->RequestPlaySession(Params);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new FVTGHistoryScenario(this)));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    FAutomationTestFramework::Get().EnqueueLatentCommand(RestoreSettings);
    return true;
}
#endif
