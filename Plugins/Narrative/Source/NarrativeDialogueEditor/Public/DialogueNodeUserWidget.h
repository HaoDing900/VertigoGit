//  Copyright Narrative Tools 2022.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogueNodeUserWidget.generated.h"

/**
 * Parent class for a custom UMG widget that narrative will add to dialogue nodes if you want to override narratives default UI
 */
UCLASS()
class NARRATIVEDIALOGUEEDITOR_API UDialogueNodeUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	void InitializeFromNode(class UDialogueNode* InNode, class UDialogue* InDialogue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue Node")
	void OnNodeInitialized(class UDialogueNode* InNode, class UDialogue* InDialogue);

	/**
	 * Returns "PPE: <image name>" when this line has a Profile Picture Expression assigned (and the dialogue's
	 * bShowPPEOnNodes is on). Empty otherwise - bind a TextBlock's Text to this; it stays blank when there's no
	 * expression. Bind that TextBlock's Visibility to GetProfilePictureExpressionVisibility to hide it cleanly.
	 */
	UFUNCTION(BlueprintPure, Category = "Dialogue Node")
	FText GetProfilePictureExpressionLabel() const;

	/** Visible when there is a PPE label to show, Collapsed otherwise. */
	UFUNCTION(BlueprintPure, Category = "Dialogue Node")
	ESlateVisibility GetProfilePictureExpressionVisibility() const;

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Node")
	class UDialogueNode* Node;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Node")
	class UDialogue* Dialogue;

public:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Dialogue Node")
	class UVerticalBox* LeftPinBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Dialogue Node")
	class UVerticalBox* RightPinBox;
};
