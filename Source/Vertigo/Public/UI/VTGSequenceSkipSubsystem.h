#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Styling/SlateBrush.h"
#include "VTGSequenceSkipSubsystem.generated.h"
class ULevelSequencePlayer;
class ULevelSequence;

/** Hold Space to conceal and fast-forward opted-in cinematics through normal Sequencer evaluation. */
UCLASS()
class VERTIGO_API UVTGSequenceSkipSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool HandleKey(const FKey& Key, bool bDown, bool bRepeat = false);
    void NotifyActivity(const FKey& Key);
    void TickInput(bool bFocused);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** Evaluate one display frame at a time, preserving triggers, repeaters and finish delegates. */
    /** Story dialogue is an interaction boundary, even before its choices are displayed. */
    static bool HasActiveDialogue(const UWorld* World);
    static bool AdvanceFrame(ULevelSequencePlayer* Player);
    static bool IsSkipAllowed(ULevelSequence* Sequence);
private:
    friend class FVTGSequenceSkipPromptRegression;
    friend class UVTGSequenceSkipCheckCommandlet;
    enum class EPhase : uint8 { Idle, Holding, FadeToBlack, FastForward, Reveal };
    EPhase Phase = EPhase::Idle;
    TArray<TWeakObjectPtr<ULevelSequencePlayer>> Targets;
    TMap<TWeakObjectPtr<ULevelSequencePlayer>, double> FrameCredits;
    bool bConsumeRelease = false;
    bool bHintVisible = false;
    FKey HeldKey;
    double HoldStart = 0;
    double HintUntil = 0;
    double FadeStart = 0;
    double LastAdvanceTime = 0;
    float BlackOpacity = 0;
    float RevealOpacity = 0;
    float HoldProgress = 0;
    UPROPERTY(Transient) FSlateBrush SkipKeyBrush;
    FDelegateHandle TravelHandle;
    bool CanOfferSkip() const;
    TArray<TWeakObjectPtr<ULevelSequencePlayer>> FindTargets() const;
    void NotifyActivityAt(const FKey& Key, double Now);
    bool HandleKeyAt(const FKey& Key, bool bDown, bool bRepeat, double Now);
    void TickAt(bool bFocused, double Now);
    void BeginReveal(double Now);
    void BeforeTravel(const FString& Map);
    void Cancel();
    void ShowHint();
    TSharedRef<class SWidget> MakeOverlay();
    void HideHint();
    void UpdateHintLayer();
    TWeakObjectPtr<class UVTGHintLayer> HintLayer;
    TSharedPtr<class SWidget> Hint;
    TWeakObjectPtr<class UGameViewportClient> HintViewport;
};
