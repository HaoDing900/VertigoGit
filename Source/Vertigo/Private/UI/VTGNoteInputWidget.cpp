#include "UI/VTGNoteInputWidget.h"
#include "NarrativeUIInputComponent.h"
#include "VTGMailReadingComponent.h"
#include "GameFramework/PlayerController.h"

void UVTGNoteInputWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (IsDesignTime()) return;
    SetIsFocusable(true);
    SetNoteInputActive(true);
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
    }
    else
    {
        if (auto* Input = InputOwner.Get()) Input->Release(this);
        InputOwner.Reset();
        if (auto* Mail = MailReadingOwner.Get()) Mail->NoteClosed(this);
        MailReadingOwner.Reset();
    }
}

