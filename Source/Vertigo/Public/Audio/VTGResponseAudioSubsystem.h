#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/SlateWrapperTypes.h"
#include "VTGResponseAudioSubsystem.generated.h"

class UAudioComponent;
class UUserWidget;
class USoundBase;
class UWidget;
class UWidgetAnimation;
class UOverlay;
class UDialogue;
class UDialogueNode_Player;
class UNarrativeComponent;
class UVTGResponseLockedBackdrop;

/** Existing response feedback state, including a short, timer-driven selection transition. */
UCLASS()
class VERTIGO_API UVTGResponseAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Hover(UUserWidget* Button);
    void Unhover(UUserWidget* Button);
    void Confirm();
    bool BeginTransition(UUserWidget* Button, UNarrativeComponent* Component, UDialogueNode_Player* Option);
    void CancelTransitionFor(UUserWidget* Button);
    bool IsTransitionPending() const { return TransitionToken != 0; }
    static float GetTransitionDuration();
    static float GetButtonFadeStart(float Duration){return Duration*(.9f-.3f/1.2f);}
    virtual void Deinitialize() override;
private:
    friend bool VTGRunResponseTransitionChecks();
    struct FWidgetState
    {
        TWeakObjectPtr<UWidget> Widget;
        ESlateVisibility Visibility;
        float Opacity;
        bool bChangedVisibility;
    };
    TArray<FWidgetState> TransitionWidgets;
    TWeakObjectPtr<UUserWidget> SelectedButton;
    TWeakObjectPtr<UDialogue> PendingDialogue;
    TWeakObjectPtr<UNarrativeComponent> PendingComponent;
    TWeakObjectPtr<UDialogueNode_Player> PendingOption;
    uint32 TransitionToken = 0;
    FTimerHandle FadeTimer;
    FTimerHandle StartFadeTimer;
    FTimerHandle SubmitTimer;
    UPROPERTY(Transient) TObjectPtr<UWidgetAnimation> ResponseFade;
    UPROPERTY(Transient) TObjectPtr<UWidgetAnimation> ConfirmAppearance;
    UPROPERTY(Transient) TObjectPtr<UVTGResponseLockedBackdrop> LockedBackdrop;
    TWeakObjectPtr<UOverlay> AppearanceOverlay;
    void StartConfirmAppearance();
    void FinishFade();
    void StartFade();
    float TransitionDuration = 0.f;
    void SubmitResponse();
    void CancelTransition();
    void RestoreTransitionUI();
    UFUNCTION() void DialogueEnded(UDialogue* Dialogue, bool bStartingNewDialogue);
    TWeakObjectPtr<UAudioComponent> HoverAudio;
    TWeakObjectPtr<UAudioComponent> ConfirmAudio;
    TWeakObjectPtr<UUserWidget> HoveredButton;
    TWeakObjectPtr<USoundBase> LastHoverSound;
    TWeakObjectPtr<USoundBase> LastConfirmSound;
    double LastHoverTime = -1.e30;
};
