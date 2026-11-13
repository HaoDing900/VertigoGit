#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "VTGTerminalWidget.generated.h"

/** Per-player input gate; rejected presses are discarded, never queued. */
UCLASS()
class VERTIGO_API UVTGTerminalInputGate : public UActorComponent
{
 GENERATED_BODY()
public:
 bool TryAccept();
private:
 double NextInputTime=-1;
};

UCLASS()
class VERTIGO_API UVTGTerminalWidget : public UUserWidget
{
 GENERATED_BODY()
 friend struct FVTGTerminalSaveRegression;
public:
 void SynchronizeInput(APlayerController* Player);
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativeDestruct() override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
 UFUNCTION() void TerminalVisibilityChanged(ESlateVisibility InVisibility);
 void ReleaseInput();
 void FinishOpeningFocus(float DeltaTime);
 void CancelOpeningFocus();
 FDelegateHandle OpeningFocusHandle;
 TWeakObjectPtr<class UNarrativeUIInputComponent> InputOwner;
 bool OwnsInput=false;
};
