#include "VTGPlayerController.h"
#include "NarrativeUIInputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

void AVTGPlayerController::SetInputMode(const FInputModeDataBase& InData)
{
    Super::SetInputMode(InData);
    const auto* Local = GetLocalPlayer();
    auto* Input = UNarrativeUIInputComponent::Find(this);
    // Inspect the applied mode's actual behavior, not a debug-only type name (also works in Shipping).
    // UI-only requests from pause/history retain their own focus; gameplay requests cannot unlock a modal.
    if (Input && Input->HasOwners() && Local && Local->ViewportClient &&
        Local->ViewportClient->GetGameViewportWidget().IsValid() && !Local->ViewportClient->IgnoreInput())
        Input->Refresh();
}

bool AVTGPlayerController::ShouldShowMouseCursor() const
{
    const auto* Input = UNarrativeUIInputComponent::Find(const_cast<AVTGPlayerController*>(this));
    return (Input && Input->HasOwners()) || Super::ShouldShowMouseCursor();
}
