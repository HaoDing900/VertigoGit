#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NarrativeActivatableWidget.h"
#include "VTGDialogueHistorySubsystem.generated.h"
class UNarrativeComponent;
class UFont;
class IInputProcessor;
class SScrollBox;
class SWidget;

USTRUCT(BlueprintType)
struct FVTGDialogueLogEntry
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FText Speaker;
    UPROPERTY(BlueprintReadOnly) FText Text;
};

/** Session history, retained across level changes. Only actually presented lines are recorded. */
UCLASS()
class VERTIGO_API UVTGDialogueHistorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category = "Dialogue History") void ShowHistory();
    UFUNCTION(BlueprintCallable, Category = "Dialogue History") void HideHistory();
    UFUNCTION(BlueprintPure, Category = "Dialogue History") bool IsHistoryOpen() const
    {
        return Panel.IsValid();
    }
    UFUNCTION(BlueprintPure, Category = "Dialogue History") bool IsInDialogue() const;
    UPROPERTY(BlueprintReadOnly, Category = "Dialogue History") TArray<FVTGDialogueLogEntry> Entries;
    UFUNCTION(BlueprintCallable, Category="Dialogue") void ToggleAutoAdvance(FName ActionName = NAME_None);
    UFUNCTION(BlueprintPure, Category="Dialogue") bool IsAutoAdvanceEnabled() const { return bAutoAdvance; }
    void TickControls();
    void SetSkipHeld(bool bHeld);
    bool IsSkipHeld() const { return bSkipHeld; }
    void TickSkip(double Now);
    bool HandleSkipKey(const FKey& Key, bool bPressed, bool bRepeat = false);
    bool CanHandleInput() const;
    bool HandleEscape();
    bool HandleWheel(float Delta);
    void ScrollHistory(float WheelDelta);
    void RecordLine(UNarrativeComponent *Component, const FText &Speaker, const FText &Text);

  private:
    bool bSkipHeld = false;
    double NextSkipTime = 0;
    TWeakObjectPtr<UNarrativeComponent> SkipComponent;
    TWeakObjectPtr<class UDialogue> SkipDialogue;
    TWeakObjectPtr<class UCommonButtonBase> SkipButton;
    UFUNCTION() void SkipPromptClicked(FName ActionName);
    void BeginMouseSkip();
    void EndMouseSkip();
    void BindSkipButton();
    void UpdatePromptLayout();
    void RestorePromptLayout();
    void UpdatePromptBarVisibility(bool bShowDockedBar);
    void RestorePromptBarVisibility();
    TMap<TWeakObjectPtr<class UCommonBoundActionBar>, ESlateVisibility> SuppressedPromptBars;
    UPROPERTY(Transient) TObjectPtr<class UCommonBoundActionBar> DockedActionBar;
    UPROPERTY(Transient) TObjectPtr<class UOverlay> OriginalActionParent;
    UPROPERTY(Transient) TObjectPtr<class UOverlaySlot> OriginalActionSlot;
    UPROPERTY(Transient) TObjectPtr<class UScaleBox> PromptFooter;
    UPROPERTY(Transient) TObjectPtr<class UCanvasPanel> PromptSurface;
    TWeakObjectPtr<class UUserWidget> PromptOverlay;
    bool bAutoAdvance = true;
    UFUNCTION() void OpenHistoryAction(FName ActionName);
    FInputActionBindingHandle AutoHandle;
    TWeakObjectPtr<UNarrativeActivatableWidget> BoundMenu;
    UPROPERTY(Transient) TObjectPtr<UDataTable> ControlsTable;
    UPROPERTY(Transient) TObjectPtr<class UUserWidget> DialoguePauseMenu;
    TWeakPtr<SWidget> PrePauseFocus;
    bool bPrePauseCursor = false;
    bool bPrePauseAllowCursor = false;
    void RestoreDialogueInput();
    void RebuildRows();
    void BeforeTravel(const FString &Map);
    FDelegateHandle LineHandle;
    FDelegateHandle TravelHandle;
    TSharedPtr<IInputProcessor> Input;
    TSharedPtr<SWidget> Panel;
    TWeakPtr<SWidget> PreviousFocus;
    TSharedPtr<SScrollBox> Scroll;
    TWeakObjectPtr<class UGameViewportClient> PanelViewport;
    UPROPERTY(Transient) TObjectPtr<UFont> Font;
};
