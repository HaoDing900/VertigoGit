#include "UI/VTGNoteInputWidget.h"
#include "NarrativeUIInputComponent.h"
#include "VTGMailReadingComponent.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UnrealType.h"

void UVTGNoteInputWidget::CancelOpeningFocus()
{
    if (OpeningFocusHandle.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().OnPostTick().Remove(OpeningFocusHandle);
    OpeningFocusHandle.Reset();
}

void UVTGNoteInputWidget::FinishOpeningFocus(float DeltaTime)
{
    CancelOpeningFocus();
    auto* PC = GetOwningPlayer();
    auto* Input = InputOwner.Get();
    if (!PC || !Input || Input->GetActiveOwner() != this || !IsVisible()) return;
    const auto SlateRoot = GetCachedWidget();
    // An active Note can be embedded in the inventory's widget tree rather than
    // added to the viewport itself. Still require attachment before focusing it.
    if (!SlateRoot.IsValid() || !FSlateApplication::Get().FindWidgetWindow(SlateRoot.ToSharedRef()).IsValid()) return;
    // Construct/Note state callbacks can run before Slate attaches the widget. CommonUI
    // also applies its pending focus during this frame; finish the handoff after that.
    // The root handles tab switching; the active reading panel handles page/back/close.
    // Focusing the root cannot deliver keys down to that panel.
    UUserWidget* Target = this;
    if (const auto* Property = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("ActiveWidget")))
        if (auto* Active = Cast<UUserWidget>(Property->GetObjectPropertyValue_InContainer(this)))
            if (Active->IsVisible()) Target = Active;
    Target->SetIsFocusable(true);
    Input->Acquire(this, Target, 100);
    if (!Target->HasUserFocus(PC) && !Target->HasUserFocusedDescendants(PC))
    {
        Target->SetUserFocus(PC);
    }
}

void UVTGNoteInputWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (IsDesignTime()) return;
    SetIsFocusable(true);
    // Inventory preconstructs and caches this widget even when no Note is open.
    // Only the NotesComponent's logical state callback may acquire input.
}

void UVTGNoteInputWidget::NativeDestruct()
{
    Super::NativeDestruct();
    SetNoteInputActive(false);
}

void UVTGNoteInputWidget::SetNoteInputActive(bool bActive)
{
    if (IsDesignTime()) return;
    if (bActive)
    {
        if (auto* PC = GetOwningPlayer())
            if (auto* Mail = UVTGMailReadingComponent::Find(PC->GetPawn()))
            {
                MailReadingOwner = Mail;
                Mail->NoteOpened(this);
            }
        if (auto* Input = UNarrativeUIInputComponent::GetOrCreate(GetOwningPlayer()))
        {
            InputOwner = Input;
            Input->Acquire(this, this, 100);
        }
        CancelOpeningFocus();
        if (FSlateApplication::IsInitialized())
            OpeningFocusHandle = FSlateApplication::Get().OnPostTick().AddUObject(this, &UVTGNoteInputWidget::FinishOpeningFocus);
    }
    else
    {
        CancelOpeningFocus();
        if (auto* Input = InputOwner.Get()) Input->Release(this);
        InputOwner.Reset();
        if (auto* Mail = MailReadingOwner.Get()) Mail->NoteClosed(this);
        MailReadingOwner.Reset();
    }
}

