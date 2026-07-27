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

	/**
	 * Counter-scales the events text against the graph zoom so it stays readable. The title (TB_EventsTitle,
	 * yellow "EVENTS") and the content (TB_Events) use separate zoomed-in/zoomed-out sizes offset from
	 * EventsTextFontSize - see the specs table in the .cpp. Called every frame by SDialogueGraphNode with the
	 * owner panel's zoom; cheap no-op when the zoom alpha is unchanged.
	 */
	void UpdateEventsFontForZoom(float ZoomAmount);

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

	/**
	 * Returns "RP: <image name>" when this line has a Response Picture assigned. Empty otherwise - bind a
	 * TextBlock's Text to this; it stays blank when there's no picture. Bind that TextBlock's Visibility
	 * to GetResponsePictureVisibility to hide it cleanly. Editor display only.
	 */
	UFUNCTION(BlueprintPure, Category = "Dialogue Node")
	FText GetResponsePictureLabel() const;

	/** Visible when there is a Response Picture label to show, Collapsed otherwise. */
	UFUNCTION(BlueprintPure, Category = "Dialogue Node")
	ESlateVisibility GetResponsePictureVisibility() const;

	/**
	 * Body text for the node: the line's Text, plus a player option's typed Hint Text in [brackets].
	 * Shows "Text [Hint]", or just "[Hint]" when Text is empty, or Text alone when there's no hint. Bind your
	 * node widget's main text block to this so a hint-only player option isn't blank. Editor display only -
	 * does not affect runtime dialogue UI.
	 */
	UFUNCTION(BlueprintPure, Category = "Dialogue Node")
	FText GetNodeBodyText() const;

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Node")
	class UDialogueNode* Node;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Node")
	class UDialogue* Dialogue;

	//Last zoom alpha applied by UpdateEventsFontForZoom, so we only touch the text blocks when it changes
	float LastAppliedEventsAlpha = -1.f;

public:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Dialogue Node")
	class UVerticalBox* LeftPinBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Dialogue Node")
	class UVerticalBox* RightPinBox;
};
