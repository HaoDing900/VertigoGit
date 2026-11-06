#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGUIFeedbackLibrary.generated.h"

class USoundBase;
class UUserWidget;
class UDialogue;
class UDialogueNode_Player;
class UNarrativeComponent;
class UTexture2D;

/** Interaction and quest feedback, independent of Narrative dialogue voice audio. */
UCLASS()
class VERTIGO_API UVTGUIFeedbackLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UVTGUIFeedbackLibrary();

    UFUNCTION(BlueprintCallable, Category="Audio|Feedback", meta=(WorldContext="WorldContextObject"))
    static void PlayInteractionFeedback(UObject* WorldContextObject, bool bEnabled, USoundBase* SoundOverride);

    UFUNCTION(BlueprintCallable, Category="Audio|Feedback", meta=(WorldContext="WorldContextObject"))
    static void PlayQuestFeedback(UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category="Audio|Feedback")
    static void HoverResponse(UUserWidget* Button, UDialogue* Dialogue, UDialogueNode_Player* Option);

    UFUNCTION(BlueprintCallable, Category="Audio|Feedback")
    static void UnhoverResponse(UUserWidget* Button);

    UFUNCTION(BlueprintCallable, Category="Audio|Feedback")
    static void ReleaseResponseWidget(UUserWidget* Button);
    UFUNCTION(BlueprintPure, Category="Audio|Feedback")
    static bool CanUpdateResponseAppearance(UUserWidget* Button);
    UFUNCTION(BlueprintPure, Category="Dialogue|Presentation")
    static FText FilterSpeakerLabel(FName SpeakerID, FText SpeakerName, FText Label);

    UFUNCTION(BlueprintCallable, Category="Audio|Feedback")
    static void ConfirmResponse(UUserWidget* Button, UNarrativeComponent* NarrativeComponent, UDialogueNode_Player* Option);

    static USoundBase* ResolveInteractionSound(bool bEnabled, USoundBase* SoundOverride);
    UPROPERTY() TArray<TObjectPtr<USoundBase>> InteractionSounds;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> QuestSounds;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> ResponseSounds;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> ResponseHoverSounds;
    UPROPERTY() TObjectPtr<UTexture2D> ResponseLockedTexture;
};
