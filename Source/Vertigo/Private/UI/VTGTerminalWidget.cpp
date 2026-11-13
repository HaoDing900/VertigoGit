#include "UI/VTGTerminalWidget.h"
#include "UI/VTGTerminalSaveLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "NarrativeUIInputComponent.h"
#include "Framework/Application/SlateApplication.h"

void UVTGTerminalWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 OnVisibilityChanged.AddUniqueDynamic(this,&UVTGTerminalWidget::TerminalVisibilityChanged);
}

void UVTGTerminalWidget::SynchronizeInput(APlayerController* Player)
{
 if(!Player || !Player->IsLocalController())return;
 SetOwningPlayer(Player);
 if(GetVisibility()==ESlateVisibility::Collapsed || GetVisibility()==ESlateVisibility::Hidden)
 {ReleaseInput();return;}
 // Inventory widgets can be created without an OwningPlayer, and the plugin's
 // input-mode branch is configurable. Explicitly focus this terminal, not its
 // inventory sibling, after the complete open call has returned.
 SetIsFocusable(true);
 OwnsInput=true;
 // CommonUI's deferred input configuration can run after this function. Register
 // the terminal with the same ownership mechanism as dialogue/Note so the real
 // player controller rejects a later request to return to gameplay while open.
 auto* Input=UNarrativeUIInputComponent::GetOrCreate(Player);InputOwner=Input;
 if(Input)Input->Acquire(this,this,50);
 if(Player->PlayerInput)Player->PlayerInput->FlushPressedKeys();
 SetUserFocus(Player);
 CancelOpeningFocus();
 if(FSlateApplication::IsInitialized())OpeningFocusHandle=FSlateApplication::Get().OnPostTick().AddUObject(this,&UVTGTerminalWidget::FinishOpeningFocus);
}

void UVTGTerminalWidget::CancelOpeningFocus()
{
 if(OpeningFocusHandle.IsValid() && FSlateApplication::IsInitialized())FSlateApplication::Get().OnPostTick().Remove(OpeningFocusHandle);
 OpeningFocusHandle.Reset();
}

void UVTGTerminalWidget::FinishOpeningFocus(float DeltaTime)
{
 CancelOpeningFocus();
 auto* Input=InputOwner.Get();auto* Player=GetOwningPlayer();
 if(!OwnsInput || !Input || Input->GetActiveOwner()!=this || !IsVisible() || !Player)return;
 auto Slate=GetCachedWidget();
 if(!Slate.IsValid() || !FSlateApplication::Get().FindWidgetWindow(Slate.ToSharedRef()).IsValid())return;
 Input->Refresh();SetUserFocus(Player);
}

void UVTGTerminalWidget::ReleaseInput()
{
 if(!OwnsInput)return;
 OwnsInput=false;
 CancelOpeningFocus();
 if(auto* Input=InputOwner.Get())Input->Release(this);
 InputOwner.Reset();
 if(auto* Player=GetOwningPlayer())
 {
  if(Player->PlayerInput)Player->PlayerInput->FlushPressedKeys();
 }
}

void UVTGTerminalWidget::TerminalVisibilityChanged(ESlateVisibility InVisibility)
{
 if(InVisibility==ESlateVisibility::Collapsed || InVisibility==ESlateVisibility::Hidden)ReleaseInput();
}

void UVTGTerminalWidget::NativeDestruct()
{
 ReleaseInput();Super::NativeDestruct();
}

bool UVTGTerminalInputGate::TryAccept()
{
 const auto* World=GetWorld();
 if(!World || World->GetTimeSeconds()<1.5)return false;
 const double Now=World->GetRealTimeSeconds();
 if(Now<NextInputTime)return false;
 NextInputTime=Now+.25;
 return true;
}

FReply UVTGTerminalWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 if(Event.GetKey()==EKeys::Tab)
 {
  if(Event.IsRepeat() || !UVTGTerminalSaveLibrary::TryAcceptTerminalInput(GetOwningPlayerPawn()))return FReply::Handled();
  // Handle Tab before a page can consume it. The authored Close function keeps
  // the HUD's own visibility/status cleanup, and its visibility signal releases
  // input ownership for mouse, keyboard and Save / Load close routes alike.
  if(auto* Close=FindFunction(TEXT("Close")))ProcessEvent(Close,nullptr);
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
