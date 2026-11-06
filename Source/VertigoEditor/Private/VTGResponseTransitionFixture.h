#pragma once
#include "Blueprint/UserWidget.h"
#include "VTGResponseTransitionFixture.generated.h"
class UScrollBox;
class URichTextBlock;
class UNarrativeComponent;
UCLASS()
class UVTGResponseTransitionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UUserWidget> OwningWidget;
    UPROPERTY() TObjectPtr<UScrollBox> SB_PlayerDialogueOptions;
    UPROPERTY() TObjectPtr<UNarrativeComponent> NarrativeComponent;
    UPROPERTY() TObjectPtr<URichTextBlock> Line_Main;
    UPROPERTY() TObjectPtr<UWidget> DialogueBox;
    UPROPERTY() TObjectPtr<UWidget> AvtProfile_Main;
};
bool VTGRunResponseTransitionChecks();
