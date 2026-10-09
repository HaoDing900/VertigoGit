#include "UI/VTGDialogueHistorySubsystem.h"
#include "UI/VTGSequenceSkipSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Input/CommonBoundActionBar.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "DialogueHistoryEvents.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Regex.h"
#include "NarrativeComponent.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace VTGDialogueLog
{
class FInput : public IInputProcessor
{
  public:
    explicit FInput(UVTGDialogueHistorySubsystem *InOwner) : Owner(InOwner)
    {
    }
    virtual void Tick(float, FSlateApplication &, TSharedRef<ICursor>) override
    {
        if (Owner.IsValid()) Owner->TickControls();
    }
    virtual bool HandleKeyDownEvent(FSlateApplication &, const FKeyEvent &Event) override
    {
        if (!Owner.IsValid() || !Owner->CanHandleInput())
            return false;
        if (Event.GetKey() == EKeys::Escape && Owner->GetGameInstance()->GetSubsystem<UVTGSequenceSkipSubsystem>()->HandleEscape(true, Event.IsRepeat())) return true;
        if (Owner->HandleSkipKey(Event.GetKey(), true, Event.IsRepeat())) return true;
        if (Event.GetKey() == EKeys::Escape && !Event.IsRepeat() && Owner->HandleEscape())
        {
            bConsumeEscapeUp = true;

            return true;
        }
        if (Owner->IsInDialogue() && UGameplayStatics::IsGamePaused(Owner.Get()) &&
            (Event.GetKey() == EKeys::SpaceBar || Event.GetKey() == EKeys::A ||
             Event.GetKey() == EKeys::Gamepad_LeftShoulder || Event.GetKey() == EKeys::Gamepad_RightShoulder)) return true;
        return Owner->IsHistoryOpen();
    }
    virtual bool HandleKeyUpEvent(FSlateApplication &, const FKeyEvent &Event) override
    {
        if (Owner.IsValid() && Event.GetKey() == EKeys::Escape && Owner->GetGameInstance()->GetSubsystem<UVTGSequenceSkipSubsystem>()->HandleEscape(false)) return true;
        if (Owner.IsValid() && Owner->HandleSkipKey(Event.GetKey(), false)) return true;
        if (Event.GetKey() == EKeys::Escape && bConsumeEscapeUp)
        {
            bConsumeEscapeUp = false;
            return true;
        }
        return Owner.IsValid() && Owner->CanHandleInput() && Owner->IsHistoryOpen();
    }
    virtual bool HandleMouseButtonUpEvent(FSlateApplication &, const FPointerEvent& Event) override
    {
        if (Owner.IsValid() && Event.GetEffectingButton() == EKeys::LeftMouseButton) Owner->SetSkipHeld(false);
        return false;
    }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication &, const FPointerEvent &Event,
                                                const FPointerEvent *) override
    {
        if (!Owner.IsValid() || !Owner->CanHandleInput())
            return false;
        return Owner->HandleWheel(Event.GetWheelDelta());
    }

  private:
    TWeakObjectPtr<UVTGDialogueHistorySubsystem> Owner;
    bool bConsumeEscapeUp = false;
};

// IsInViewport only recognizes a top-level UUserWidget. Narrative overlays
// may instead be children of a HUD; validate their complete live parent chain.
bool IsMounted(UWidget* Widget)
{
    TSet<UWidget*> Visited;
    float EffectiveOpacity = 1.f;
    while (Widget && !Visited.Contains(Widget))
    {
        Visited.Add(Widget);
        EffectiveOpacity *= Widget->GetRenderOpacity();
        if (!Widget->IsVisible() || EffectiveOpacity <= .01f) return false;
        if (auto* UserWidget = Cast<UUserWidget>(Widget))
            if (UserWidget->IsInViewport()) return true;
        if (auto* Parent = Widget->GetParent())
        {
            Widget = Parent;
            continue;
        }
        auto* Owner = Widget->GetTypedOuter<UUserWidget>();
        // Cross a widget-tree root into its user widget, but never treat a
        // detached widget as mounted merely because its UObject outer survives.
        if (!Owner || !Owner->WidgetTree || Owner->WidgetTree->RootWidget != Widget) return false;
        Widget = Owner;
    }
    return false;
}

FText PlainText(const FText &Text)
{
    const FString Source = Text.ToString();
    FRegexMatcher Matcher(FRegexPattern(TEXT("<[^>]*>")), Source);
    FString Out;
    int32 Previous = 0;
    while (Matcher.FindNext())
    {
        Out += Source.Mid(Previous, Matcher.GetMatchBeginning() - Previous);
        Previous = Matcher.GetMatchEnding();
    }
    Out += Source.Mid(Previous);
    return FText::FromString(Out);
}
} // namespace VTGDialogueLog

void UVTGDialogueHistorySubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    LineHandle = OnNarrativeLinePresented().AddUObject(this, &UVTGDialogueHistorySubsystem::RecordLine);
    TravelHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UVTGDialogueHistorySubsystem::BeforeTravel);
    if (FSlateApplication::IsInitialized())
    {
        Input = MakeShared<VTGDialogueLog::FInput>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(Input, 0);
    }
}

void UVTGDialogueHistorySubsystem::Deinitialize()
{
    SetSkipHeld(false);
    HideHistory();
    OnNarrativeLinePresented().Remove(LineHandle);
    FCoreUObjectDelegates::PreLoadMap.Remove(TravelHandle);
    if (Input && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(Input);
    Input.Reset();
    Entries.Empty();
    RestorePromptLayout();
    RestorePromptBarVisibility();
    Super::Deinitialize();
}

bool UVTGDialogueHistorySubsystem::IsInDialogue() const
{
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
    {
        UWorld *World = It->GetWorld();
        if (!It->IsTemplate() && World && World->GetGameInstance() == GetGameInstance() &&
            World->GetNetMode() != NM_DedicatedServer && It->IsInDialogue())
            return true;
    }
    return false;
}

bool UVTGDialogueHistorySubsystem::CanHandleInput() const
{
    auto *Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport || !Viewport->Viewport || !FSlateApplication::IsInitialized())
        return false;
    const auto Widget = Viewport->GetGameViewportWidget();
    if (!Widget)
        return false;
    const auto Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef());
    if (!Window || !Window->IsActive())
        return false;
    if (GetWorld() && GetWorld()->WorldType != EWorldType::PIE)
        return true;
    const auto Focus = FSlateApplication::Get().GetUserFocusedWidget(0);
    const bool FocusInGame =
        Focus && Widget->GetCachedGeometry().IsUnderLocation(Focus->GetCachedGeometry().GetAbsolutePosition() +
                                                             Focus->GetCachedGeometry().GetAbsoluteSize() * 0.5f);
    return FocusInGame || Viewport->Viewport->HasFocus() ||
           Widget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UVTGDialogueHistorySubsystem::RecordLine(UNarrativeComponent *Component, const FText &Speaker, const FText &Text)
{
    UWorld *World = Component ? Component->GetWorld() : nullptr;
    if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_DedicatedServer ||
        Text.IsEmpty())
        return;
    if (auto* Dialogue = Component->GetCurrentDialogue()) Dialogue->SetAutoAdvance(bAutoAdvance);
    FVTGDialogueLogEntry Entry;
    Entry.Speaker = Speaker;
    Entry.Text = Text;
    Entries.Add(Entry);
    if (Entries.Num() > 500)
        Entries.RemoveAt(0, Entries.Num() - 500);
    if (Scroll)
        RebuildRows();
}

void UVTGDialogueHistorySubsystem::ShowHistory()
{
    if (Panel || !IsInDialogue() || UGameplayStatics::IsGamePaused(this))
        return;
    auto *Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport || !FSlateApplication::IsInitialized())
        return;
    SetSkipHeld(false);
    Font = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));
    FVector2D Size;
    Viewport->GetViewportSize(Size);
    Size /= FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    const float Width = FMath::Min(1100.f, float(Size.X) * .84f);
    const float Height = FMath::Min(760.f, float(Size.Y) * .82f);
    SAssignNew(Scroll, SScrollBox);
    Panel =
        SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.005f, .008f, .009f, .96f))
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .OnMouseButtonDown_Lambda([](const FGeometry &, const FPointerEvent &)
                                      { return FReply::Handled(); })[SNew(SBox).WidthOverride(Width).HeightOverride(
                Height)[SNew(SVerticalBox) +
                        SVerticalBox::Slot().AutoHeight().Padding(
                            0, 0, 0,
                            20)[SNew(SHorizontalBox) +
                                SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock)
                                                                        .Text(FText::FromString(TEXT("对话记录")))
                                                                        .Font(FSlateFontInfo(Font, 28))] +
                                SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked_Lambda(
                                    [this]()
                                    {
                                        HideHistory();
                                        return FReply::Handled();
                                    })[SNew(STextBlock)
                                           .Text(FText::FromString(TEXT("返回对话")))
                                           .Font(FSlateFontInfo(Font, 16))]]] +
                        SVerticalBox::Slot().FillHeight(1)[Scroll.ToSharedRef()] +
                        SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
                            [SNew(STextBlock)
                                 .Text(FText::FromString(TEXT("滚轮浏览记录 · 向下滚到末尾后继续滚动 / Esc 返回对话")))
                                 .Font(FSlateFontInfo(Font, 14))
                                 .ColorAndOpacity(FLinearColor(.5f, .6f, .6f, 1))]]];
    PreviousFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
    PanelViewport = Viewport;
    RebuildRows();
    Scroll->ScrollToEnd();
    Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 9000);
}

void UVTGDialogueHistorySubsystem::RebuildRows()
{
    if (!Scroll)
        return;
    const float Offset = Scroll->GetScrollOffset();
    Scroll->ClearChildren();
    if (Entries.IsEmpty())
        Scroll
            ->AddSlot()[SNew(STextBlock).Text(FText::FromString(TEXT("暂无对话记录"))).Font(FSlateFontInfo(Font, 18))];
    for (const auto &Entry : Entries)
    {
        Scroll->AddSlot().Padding(0, 0, 16,
                                  22)[SNew(SVerticalBox) +
                                      SVerticalBox::Slot().AutoHeight().Padding(
                                          0, 0, 0, 5)[SNew(STextBlock)
                                                          .Text(Entry.Speaker)
                                                          .Font(FSlateFontInfo(Font, 16))
                                                          .ColorAndOpacity(FLinearColor(.3f, .7f, .65f, 1))] +
                                      SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                                                            .Text(VTGDialogueLog::PlainText(Entry.Text))
                                                                            .Font(FSlateFontInfo(Font, 20))
                                                                            .AutoWrapText(true)]];
    }
    Scroll->SetScrollOffset(Offset);
}

void UVTGDialogueHistorySubsystem::ScrollHistory(float Delta)
{
    if (!Scroll)
        return;
    if (Delta < 0 && Scroll->GetScrollOffset() >= Scroll->GetScrollOffsetOfEnd() - 1.f)
    {
        HideHistory();
        return;
    }
    Scroll->SetScrollOffset(FMath::Max(0.f, Scroll->GetScrollOffset() - Delta * 100.f));
}

void UVTGDialogueHistorySubsystem::HideHistory()
{
    if (Panel && PanelViewport.IsValid())
        PanelViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
    if (Panel && FSlateApplication::IsInitialized())
    {
        if (auto Focus = PreviousFocus.Pin(); Focus && IsInDialogue())
            FSlateApplication::Get().SetUserFocus(0, Focus, EFocusCause::SetDirectly);
        else if (PanelViewport.IsValid())
            FSlateApplication::Get().SetUserFocus(0, PanelViewport->GetGameViewportWidget(), EFocusCause::SetDirectly);
    }
    PreviousFocus.Reset();
    Panel.Reset();
    Scroll.Reset();
    PanelViewport.Reset();
}

void UVTGDialogueHistorySubsystem::BeforeTravel(const FString &)
{
    SetSkipHeld(false);
    RestorePromptLayout();
    RestorePromptBarVisibility();
    HideHistory();
    DialoguePauseMenu = nullptr;
    PrePauseFocus.Reset();
    BoundMenu.Reset();
}

bool UVTGDialogueHistorySubsystem::HandleEscape()
{
    SetSkipHeld(false);
    if (IsHistoryOpen()) { HideHistory(); return true; }
    if (!IsInDialogue()) return false;
    if (DialoguePauseMenu && DialoguePauseMenu->IsInViewport())
    {
        DialoguePauseMenu->RemoveFromParent();
        UGameplayStatics::SetGamePaused(this, false);
        RestoreDialogueInput();
        return true;
    }
    if (UGameplayStatics::IsGamePaused(this)) return false;
    auto* PC = GetGameInstance()->GetFirstLocalPlayerController();
    if (!PC) return false;
    auto* MenuClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu.WBP_PauseMenu_C"));
    if (!MenuClass) return false;
    DialoguePauseMenu = CreateWidget<UUserWidget>(PC, MenuClass);
    if (!DialoguePauseMenu) return false;
    PrePauseFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
    bPrePauseCursor = PC->bShowMouseCursor;
    if (auto* AllowCursor = FindFProperty<FBoolProperty>(PC->GetClass(), TEXT("AllowCursorOnScreen?")))
    {
        bPrePauseAllowCursor = AllowCursor->GetPropertyValue_InContainer(PC);
        AllowCursor->SetPropertyValue_InContainer(PC, true);
    }
    if (APawn* Pawn = PC->GetPawn())
        if (auto* Prop = FindFProperty<FObjectPropertyBase>(Pawn->GetClass(), TEXT("PauseMenu")))
            Prop->SetObjectPropertyValue_InContainer(Pawn, DialoguePauseMenu);
    DialoguePauseMenu->SetIsFocusable(true);
    DialoguePauseMenu->AddToViewport(10000);
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == GetWorld() && It->IsInDialogue())
            It->GetCurrentDialogue()->SetDialogueAudioPaused(true);
    UGameplayStatics::SetGamePaused(this, true);
    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(DialoguePauseMenu->TakeWidget());
    PC->SetInputMode(Mode);
    return true;
}
bool UVTGDialogueHistorySubsystem::HandleWheel(float Delta)
{
    if (UGameplayStatics::IsGamePaused(this)) return false;
    if (IsHistoryOpen())
    {
        ScrollHistory(Delta);
        return true;
    }
    if (Delta > 0 && IsInDialogue())
    {
        ShowHistory();
        return true;
    }
    return false;
}
void UVTGDialogueHistorySubsystem::RestoreDialogueInput()
{
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == GetWorld() && It->IsInDialogue())
            It->GetCurrentDialogue()->SetDialogueAudioPaused(false);
    auto* PC = GetGameInstance()->GetFirstLocalPlayerController();
    if (PC)
    {
        if (APawn* Pawn = PC->GetPawn())
            if (auto* Prop = FindFProperty<FObjectPropertyBase>(Pawn->GetClass(), TEXT("PauseMenu")))
                if (Prop->GetObjectPropertyValue_InContainer(Pawn) == DialoguePauseMenu)
                    Prop->SetObjectPropertyValue_InContainer(Pawn, nullptr);
        PC->bShowMouseCursor = bPrePauseCursor;
        if (auto* AllowCursor = FindFProperty<FBoolProperty>(PC->GetClass(), TEXT("AllowCursorOnScreen?")))
            AllowCursor->SetPropertyValue_InContainer(PC, bPrePauseAllowCursor);
        if (IsInDialogue())
        {
            FInputModeUIOnly Mode;
            if (auto Focus = PrePauseFocus.Pin()) Mode.SetWidgetToFocus(Focus);
            PC->SetInputMode(Mode);
        }
    }
    PrePauseFocus.Reset();
    DialoguePauseMenu = nullptr;
}

void UVTGDialogueHistorySubsystem::ToggleAutoAdvance(FName)
{
    if (!IsInDialogue() || IsHistoryOpen() || UGameplayStatics::IsGamePaused(this)) return;
    bAutoAdvance = !bAutoAdvance;
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == GetWorld() && It->IsInDialogue())
            It->GetCurrentDialogue()->SetAutoAdvance(bAutoAdvance);
    if (BoundMenu.IsValid()) BoundMenu->SetBindingDisplayName(AutoHandle,
        FText::FromString(bAutoAdvance ? TEXT("Auto: ON") : TEXT("Auto: OFF")));
}

void UVTGDialogueHistorySubsystem::OpenHistoryAction(FName) { ShowHistory(); }

void UVTGDialogueHistorySubsystem::TickControls()
{
    GetGameInstance()->GetSubsystem<UVTGSequenceSkipSubsystem>()->TickInput(CanHandleInput());
    UpdatePromptLayout();
    BindSkipButton();
    if (bSkipHeld && !CanHandleInput()) SetSkipHeld(false);
    TickSkip(FPlatformTime::Seconds());
    if (DialoguePauseMenu && !DialoguePauseMenu->IsInViewport()) RestoreDialogueInput();
    if (!IsInDialogue()) return;
    for (TObjectIterator<UNarrativeActivatableWidget> It; It; ++It)
    {
        if (It->IsTemplate() || It->GetWorld() != GetWorld() || !It->IsActivated() ||
            It->GetClass()->GetFName() != TEXT("W_NarrativeMenu_Dialogue_C")) continue;
        if (BoundMenu.Get() == *It && AutoHandle.Handle.IsValid()) return;
        if (!ControlsTable) ControlsTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Narrative/NarrativeUI/DT_DialogueControls"));
        if (!ControlsTable) return;
        BoundMenu = *It;
        FDataTableRowHandle Row;
        Row.DataTable = ControlsTable;
        Row.RowName = TEXT("ToggleAuto");
        FInputActionExecutedDelegate Callback;
        Callback.BindDynamic(this, &UVTGDialogueHistorySubsystem::ToggleAutoAdvance);
        It->RegisterBinding(Row, Callback, AutoHandle, FText::FromString(bAutoAdvance ? TEXT("Auto: ON") : TEXT("Auto: OFF")));
        Row.RowName = TEXT("History");
        Callback.BindDynamic(this, &UVTGDialogueHistorySubsystem::OpenHistoryAction);
        FInputActionBindingHandle HistoryHandle;
        It->RegisterBinding(Row, Callback, HistoryHandle, FText::FromString(TEXT("History")));
        Row.RowName = TEXT("SkipHold");
        Callback.BindDynamic(this, &UVTGDialogueHistorySubsystem::SkipPromptClicked);
        FInputActionBindingHandle SkipHandle;
        It->RegisterBinding(Row, Callback, SkipHandle, FText::FromString(TEXT("Skip (Hold)")));
        return;
    }
}

void UVTGDialogueHistorySubsystem::UpdatePromptBarVisibility(bool bShowDockedBar)
{
    // No fallback to the screen corner: dialogue prompts belong exclusively
    // to a visible dialogue frame, including when dialogue is still running.
    for (TObjectIterator<UCommonBoundActionBar> It; It; ++It)
    {
        if (It->IsTemplate() || It->GetWorld() != GetWorld() ||
            It->GetOwningLocalPlayer() != GetGameInstance()->GetFirstGamePlayer()) continue;
        auto* Owner = It->GetTypedOuter<UUserWidget>();
        if (!Owner || Owner->GetClass()->GetFName() != TEXT("WBP_NarrativeHUD_C")) continue;
        if (!SuppressedPromptBars.Contains(*It)) SuppressedPromptBars.Add(*It, It->GetVisibility());
        const bool bShow = bShowDockedBar && *It == DockedActionBar;
        It->SetVisibility(bShow ? SuppressedPromptBars.FindChecked(*It) : ESlateVisibility::Collapsed);
    }
}

void UVTGDialogueHistorySubsystem::RestorePromptBarVisibility()
{
    for (const auto& Item : SuppressedPromptBars)
        if (Item.Key.IsValid()) Item.Key->SetVisibility(Item.Value);
    SuppressedPromptBars.Empty();
}

void UVTGDialogueHistorySubsystem::RestorePromptLayout()
{
    if (DockedActionBar && OriginalActionParent && OriginalActionSlot)
    {
        DockedActionBar->RemoveFromParent();
        auto* Slot = OriginalActionParent->AddChildToOverlay(DockedActionBar);
        Slot->SetPadding(OriginalActionSlot->GetPadding());
        Slot->SetHorizontalAlignment(OriginalActionSlot->GetHorizontalAlignment());
        Slot->SetVerticalAlignment(OriginalActionSlot->GetVerticalAlignment());
    }
    if (PromptSurface) PromptSurface->RemoveFromParent();
    DockedActionBar = nullptr;
    OriginalActionParent = nullptr;
    OriginalActionSlot = nullptr;
    PromptFooter = nullptr;
    PromptSurface = nullptr;
    PromptOverlay.Reset();
}

void UVTGDialogueHistorySubsystem::UpdatePromptLayout()
{
    if (!IsInDialogue()) { RestorePromptLayout(); UpdatePromptBarVisibility(false); return; }
    if (!PromptOverlay.IsValid() || !VTGDialogueLog::IsMounted(PromptOverlay.Get()))
    {
        RestorePromptLayout();
        for (TObjectIterator<UUserWidget> It; It; ++It)
            if (!It->IsTemplate() && It->GetWorld() == GetWorld() &&
                It->GetClass()->GetFName() == TEXT("BP_Narrative3Overlay_C") && VTGDialogueLog::IsMounted(*It))
            { PromptOverlay = *It; break; }
    }
    auto* Overlay = PromptOverlay.Get();
    auto* Canvas = Overlay && Overlay->WidgetTree ? Cast<UCanvasPanel>(Overlay->WidgetTree->RootWidget) : nullptr;
    if (!Canvas) { UpdatePromptBarVisibility(false); return; }
    // Read authored layout, never last frame's painted geometry (which can include a different DPI/transform).
    UWidget* Frame = nullptr;
    for (const TCHAR* Name : {TEXT("DialogueBox"), TEXT("MonologueBox"), TEXT("ThoughtBox"), TEXT("MailBox"), TEXT("DialogueBox_Side")})
    {
        UWidget* Candidate = Overlay->GetWidgetFromName(Name);
        if (Candidate && VTGDialogueLog::IsMounted(Candidate))
        { Frame = Candidate; break; }
    }
    if (!Frame)
    {
        RestorePromptLayout();
        UpdatePromptBarVisibility(false);
        return;
    }
    auto* FrameSlot = Cast<UCanvasPanelSlot>(Frame->Slot);
    if (!FrameSlot) { RestorePromptLayout(); UpdatePromptBarVisibility(false); return; }
    if (!DockedActionBar)
    {
        for (TObjectIterator<UCommonBoundActionBar> It; It; ++It)
        {
            if (It->IsTemplate() || It->GetWorld() != GetWorld()) continue;
            auto* Parent = Cast<UOverlay>(It->GetParent());
            auto* Slot = Cast<UOverlaySlot>(It->Slot);
            auto* Owner = It->GetTypedOuter<UUserWidget>();
            if (!Parent || !Slot || !Owner || !VTGDialogueLog::IsMounted(Owner) || Owner->GetClass()->GetFName() != TEXT("WBP_NarrativeHUD_C")) continue;
            DockedActionBar = *It;
            OriginalActionParent = Parent;
            OriginalActionSlot = Slot;
            PromptFooter = Overlay->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("VTG_DialoguePromptFooter"));
            PromptFooter->SetStretch(EStretch::ScaleToFit);
            PromptFooter->SetStretchDirection(EStretchDirection::DownOnly);
            PromptFooter->SetClipping(EWidgetClipping::ClipToBounds);
            PromptSurface = Overlay->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("VTG_DialoguePromptSurface"));
            PromptSurface->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            auto* SurfaceSlot = Canvas->AddChildToCanvas(PromptSurface);
            SurfaceSlot->SetZOrder(FrameSlot->GetZOrder() + 1);
            auto* FooterSlot = PromptSurface->AddChildToCanvas(PromptFooter);
            FooterSlot->SetAnchors(FAnchors(.30f, .64f, .95f, .80f));
            FooterSlot->SetOffsets(FMargin(0));
            FooterSlot->SetAlignment(FVector2D::ZeroVector);
            FooterSlot->SetAutoSize(false);
            DockedActionBar->RemoveFromParent();
            auto* Inner = Cast<UScaleBoxSlot>(PromptFooter->AddChild(DockedActionBar));
            Inner->SetHorizontalAlignment(HAlign_Right);
            Inner->SetVerticalAlignment(VAlign_Center);
            break;
        }
    }
    if (!PromptFooter) { UpdatePromptBarVisibility(false); return; }
    UpdatePromptBarVisibility(true);
    // Both surfaces are arranged by the same canvas in the same pass. Stretch anchors,
    // alignment, DPI, parent scaling and the artist's render transform therefore match.
    // Keep Frame's existing slot intact: Blueprint animations still target that slot.
    auto* SurfaceSlot = CastChecked<UCanvasPanelSlot>(PromptSurface->Slot);
    SurfaceSlot->SetLayout(FrameSlot->GetLayout());
    SurfaceSlot->SetAutoSize(false);
    if (FrameSlot->GetAutoSize()) SurfaceSlot->SetSize(Frame->GetDesiredSize());
    SurfaceSlot->SetZOrder(FrameSlot->GetZOrder() + 1);
    PromptSurface->SetRenderTransformPivot(Frame->GetRenderTransformPivot());
    PromptSurface->SetRenderTransform(Frame->GetRenderTransform());
    PromptSurface->SetRenderOpacity(Frame->GetRenderOpacity());
    PromptFooter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

bool UVTGDialogueHistorySubsystem::HandleSkipKey(const FKey& Key, bool bPressed, bool bRepeat)
{
    if (Key != EKeys::LeftControl && Key != EKeys::Gamepad_FaceButton_Left) return false;
    if (!bPressed)
    {
        const bool WasHeld = bSkipHeld;
        SetSkipHeld(false); // Release must work even when focus has moved away.
        return WasHeld || IsInDialogue();
    }
    if (!IsInDialogue()) return false;
    if (!bRepeat) SetSkipHeld(true);
    return true;
}

void UVTGDialogueHistorySubsystem::SetSkipHeld(bool bHeld)
{
    if (!bHeld)
    {
        bSkipHeld = false;
        SkipComponent.Reset();
        SkipDialogue.Reset();
        return;
    }
    if (bSkipHeld || IsHistoryOpen() || UGameplayStatics::IsGamePaused(this)) return;
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
    {
        if (It->IsTemplate() || It->GetWorld() != GetWorld() || !It->IsInDialogue()) continue;
        UDialogue* Dialogue = It->GetCurrentDialogue();
        if (!Dialogue || !Dialogue->CanSkipCurrentLine()) continue;
        SkipComponent = *It;
        SkipDialogue = Dialogue;
        bSkipHeld = true;
        NextSkipTime = FPlatformTime::Seconds() + .35;
        return;
    }
}

void UVTGDialogueHistorySubsystem::TickSkip(double Now)
{
    if (!bSkipHeld) return;
    auto* Component = SkipComponent.Get();
    auto* Dialogue = SkipDialogue.Get();
    if (!Component || !Dialogue || Component->GetCurrentDialogue() != Dialogue ||
        IsHistoryOpen() || UGameplayStatics::IsGamePaused(this) || !Dialogue->CanSkipCurrentLine())
    {
        SetSkipHeld(false);
        return;
    }
    if (Now < NextSkipTime) return;
    NextSkipTime = Now + .10; // At most one line per tick; never replay a backlog after a stall.
    Component->TrySkipCurrentDialogueLine();
    if (Component->GetCurrentDialogue() != Dialogue || !Dialogue->CanSkipCurrentLine()) SetSkipHeld(false);
}

void UVTGDialogueHistorySubsystem::SkipPromptClicked(FName)
{
    // A click alone never skips a line. Mouse press/release and keyboard hold own the repeat state.
}
void UVTGDialogueHistorySubsystem::BeginMouseSkip() { SetSkipHeld(true); }
void UVTGDialogueHistorySubsystem::EndMouseSkip() { SetSkipHeld(false); }
void UVTGDialogueHistorySubsystem::BindSkipButton()
{
    if (!DockedActionBar) return;
    for (UUserWidget* Entry : DockedActionBar->GetAllEntries())
    {
        auto* Button = Cast<UCommonButtonBase>(Entry);
        auto* Label = Cast<UCommonTextBlock>(Entry->GetWidgetFromName(TEXT("Text_ActionName")));
        if (!Button || !Label || Label->GetText().ToString() != TEXT("Skip (Hold)")) continue;
        if (SkipButton.Get() == Button) return;
        if (SkipButton.IsValid())
        {
            SkipButton->OnPressed().RemoveAll(this);
            SkipButton->OnReleased().RemoveAll(this);
        }
        SkipButton = Button;
        Button->OnPressed().AddUObject(this, &UVTGDialogueHistorySubsystem::BeginMouseSkip);
        Button->OnReleased().AddUObject(this, &UVTGDialogueHistorySubsystem::EndMouseSkip);
        return;
    }
}
