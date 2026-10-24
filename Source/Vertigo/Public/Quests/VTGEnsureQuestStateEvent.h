#pragma once
#include "CoreMinimal.h"
#include "NarrativeEvent.h"
#include "VTGEnsureQuestStateEvent.generated.h"
class UQuest;

/** Start a missing quest or advance its existing instance, without restarting its saved history. */
UCLASS(DisplayName="VTG Ensure Quest State")
class VERTIGO_API UVTGEnsureQuestStateEvent : public UNarrativeEvent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Quest") TSubclassOf<UQuest> Quest;
    UPROPERTY(EditAnywhere, Category="Quest") FName StateID;
    virtual void ExecuteEvent_Implementation(APawn* Pawn,APlayerController* Controller,UNarrativeComponent* Component) override;
    virtual FString GetGraphDisplayText_Implementation() override;
};
