//  Copyright Narrative Tools 2022.


#include "DialogueNodeUserWidget.h"
#include <Components/VerticalBox.h>
#include <Components/Overlay.h>
#include <Components/VerticalBoxSlot.h>
#include <Blueprint/WidgetTree.h>
#include "Dialogue.h"
#include "DialogueSM.h"

#define LOCTEXT_NAMESPACE "DialogueNodeUserWidget"

void UDialogueNodeUserWidget::InitializeFromNode(class UDialogueNode* InNode, class UDialogue* InDialogue)
{
	if (InNode)
	{
		Node = InNode;
		Dialogue = InDialogue;

		OnNodeInitialized(InNode, InDialogue);
	}
}

FText UDialogueNodeUserWidget::GetProfilePictureExpressionLabel() const
{
	//Respect the per-dialogue toggle (Class Defaults -> Show PPE On Nodes).
	if (Dialogue && !Dialogue->bShowPPEOnNodes)
	{
		return FText::GetEmpty();
	}

	if (Node && Node->Line.ProfilePictureExpression)
	{
		return FText::Format(LOCTEXT("PPELabel", "PPE: {0}"), FText::FromString(Node->Line.ProfilePictureExpression->GetName()));
	}

	return FText::GetEmpty();
}

ESlateVisibility UDialogueNodeUserWidget::GetProfilePictureExpressionVisibility() const
{
	return GetProfilePictureExpressionLabel().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
}

#undef LOCTEXT_NAMESPACE
