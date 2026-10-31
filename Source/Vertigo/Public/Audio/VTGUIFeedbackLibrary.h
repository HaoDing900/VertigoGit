#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGUIFeedbackLibrary.generated.h"

class USoundBase;

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

    static USoundBase* ResolveInteractionSound(bool bEnabled, USoundBase* SoundOverride);
    UPROPERTY() TArray<TObjectPtr<USoundBase>> InteractionSounds;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> QuestSounds;
};
