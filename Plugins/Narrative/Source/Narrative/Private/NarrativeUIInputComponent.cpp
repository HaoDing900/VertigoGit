#include "NarrativeUIInputComponent.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

UNarrativeUIInputComponent* UNarrativeUIInputComponent::Find(APlayerController* Controller)
{
    return Controller ? Controller->FindComponentByClass<UNarrativeUIInputComponent>() : nullptr;
}

UNarrativeUIInputComponent* UNarrativeUIInputComponent::GetOrCreate(APlayerController* Controller)
{
    if (!Controller || !Controller->IsLocalController()) return nullptr;
    if (auto* Existing = Find(Controller)) return Existing;
    auto* Component = NewObject<UNarrativeUIInputComponent>(Controller);
    Controller->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return Component;
}

UNarrativeUIInputComponent::FRequest* UNarrativeUIInputComponent::GetActiveRequest()
{
    FRequest* Active = nullptr;
    for (auto& Request : Requests)
        if (Request.Owner.IsValid() && (!Active || Request.Priority > Active->Priority ||
            (Request.Priority == Active->Priority && Request.Order > Active->Order))) Active = &Request;
    return Active;
}

UObject* UNarrativeUIInputComponent::GetActiveOwner() const
{
    auto* Active = const_cast<UNarrativeUIInputComponent*>(this)->GetActiveRequest();
    return Active ? Active->Owner.Get() : nullptr;
}

bool UNarrativeUIInputComponent::HasOwners() const { return GetActiveOwner() != nullptr; }

void UNarrativeUIInputComponent::Acquire(UObject* Owner, UUserWidget* FocusWidget, int32 Priority)
{
    if (!IsValid(Owner)) return;
    auto* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller || !Controller->IsLocalController()) return;
    // Save the dialogue's focus before the note takes it, so closing a note can return to it.
    if (auto* Active = GetActiveRequest())
        if (Active->Owner != Owner && Priority >= Active->Priority && FSlateApplication::IsInitialized())
            Active->SavedFocus = FSlateApplication::Get().GetUserFocusedWidget(Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->GetControllerId() : 0);
    FRequest* Request = Requests.FindByPredicate([Owner](const FRequest& Entry) { return Entry.Owner == Owner; });
    if (!Request) Request = &Requests.AddDefaulted_GetRef();
    Request->Owner = Owner;
    Request->Widget = FocusWidget;
    Request->Priority = Priority;
    Request->Order = ++NextOrder;
    Refresh();
}

void UNarrativeUIInputComponent::Release(UObject* Owner)
{
    const int32 Removed = Requests.RemoveAll([Owner](const FRequest& Entry) { return !Entry.Owner.IsValid() || Entry.Owner == Owner; });
    if (Removed) Refresh();
}

void UNarrativeUIInputComponent::Refresh()
{
    auto* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller || !Controller->IsLocalController()) return;
    Requests.RemoveAll([](const FRequest& Entry) { return !Entry.Owner.IsValid(); });
    if (auto* Active = GetActiveRequest())
    {
        if (!bLockedInput)
        {
            Controller->SetIgnoreLookInput(true);
            Controller->SetIgnoreMoveInput(true);
            bLockedInput = true;
        }
        FInputModeUIOnly Mode;
        if (auto* Widget = Active->Widget.Get()) Mode.SetWidgetToFocus(Widget->TakeWidget());
        else if (auto Focus = Active->SavedFocus.Pin()) Mode.SetWidgetToFocus(Focus);
        Controller->SetInputMode(Mode);
        Controller->SetShowMouseCursor(true);
    }
    else if (bLockedInput)
    {
        Controller->SetIgnoreLookInput(false);
        Controller->SetIgnoreMoveInput(false);
        bLockedInput = false;
        Controller->SetShowMouseCursor(false);
        Controller->SetInputMode(FInputModeGameOnly());
    }
}

void UNarrativeUIInputComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Requests.Empty();
    // Balance only our own ignore-input increment, without changing a closing world's UI.
    if (bLockedInput)
        if (auto* Controller = Cast<APlayerController>(GetOwner()))
        {
            Controller->SetIgnoreLookInput(false);
            Controller->SetIgnoreMoveInput(false);
        }
    bLockedInput = false;
    Super::EndPlay(Reason);
}
