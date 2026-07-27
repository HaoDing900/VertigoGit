//  Copyright Narrative Tools 2022.


#include "DialogueNodeUserWidget.h"
#include <Components/VerticalBox.h>
#include <Components/Overlay.h>
#include <Components/VerticalBoxSlot.h>
#include <Components/TextBlock.h>
#include <Blueprint/WidgetTree.h>
#include "Dialogue.h"
#include "DialogueSM.h"
#include "DialogueEditorSettings.h"

#define LOCTEXT_NAMESPACE "DialogueNodeUserWidget"

void UDialogueNodeUserWidget::InitializeFromNode(class UDialogueNode* InNode, class UDialogue* InDialogue)
{
	if (InNode)
	{
		Node = InNode;
		Dialogue = InDialogue;

		OnNodeInitialized(InNode, InDialogue);

		//Apply the initial events font (SDialogueGraphNode keeps it in sync with the graph zoom afterwards)
		UpdateEventsFontForZoom(1.f);
	}
}

void UDialogueNodeUserWidget::UpdateEventsFontForZoom(float ZoomAmount)
{
	const UDialogueEditorSettings* Settings = GetDefault<UDialogueEditorSettings>();

	if (!Settings || Settings->EventsTextFontSize <= 0.f || !WidgetTree)
	{
		return;
	}

	//Counter-scale against zoom: at 100%+ zoom use base - Swing so the text isn't huge up close, and as the
	//graph zooms out grow smoothly up to base + Swing (fully grown by 25% zoom) so it stays readable from afar.
	const float Swing = 2.f;
	const float Alpha = FMath::Clamp((1.f - ZoomAmount) / (1.f - 0.25f), 0.f, 1.f);
	//Snap to 0.5pt steps so stepped zoom levels don't cause endless tiny font rebuilds
	const float TargetSize = FMath::GridSnap((Settings->EventsTextFontSize - Swing) + Alpha * 2.f * Swing, 0.5f);

	if (FMath::IsNearlyEqual(TargetSize, LastAppliedEventsFontSize))
	{
		return;
	}

	LastAppliedEventsFontSize = TargetSize;

	const FName EventsTextBlockNames[] = { TEXT("TB_Events"), TEXT("TB_EventsTitle") };
	for (const FName& WidgetName : EventsTextBlockNames)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(WidgetName)))
		{
			FSlateFontInfo Font = TextBlock->GetFont();
			Font.Size = TargetSize;
			TextBlock->SetFont(Font);
		}
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

FText UDialogueNodeUserWidget::GetResponsePictureLabel() const
{
	if (Node && Node->Line.ResponsePicture)
	{
		return FText::Format(LOCTEXT("ResponsePictureLabel", "RP: {0}"), FText::FromString(Node->Line.ResponsePicture->GetName()));
	}

	return FText::GetEmpty();
}

ESlateVisibility UDialogueNodeUserWidget::GetResponsePictureVisibility() const
{
	return GetResponsePictureLabel().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
}

FText UDialogueNodeUserWidget::GetNodeBodyText() const
{
	if (!Node)
	{
		return FText::GetEmpty();
	}

	const FText BodyText = Node->Line.Text;

	// For player options, also surface the typed Hint Text so a hint-only option isn't blank on the node.
	if (const UDialogueNode_Player* PlayerNode = Cast<UDialogueNode_Player>(Node))
	{
		const FText& RawHint = PlayerNode->GetRawHintText();
		if (!RawHint.IsEmptyOrWhitespace())
		{
			const FText Hint = FText::Format(INVTEXT("[{0}]"), RawHint);
			return BodyText.IsEmptyOrWhitespace() ? Hint : FText::Format(INVTEXT("{0} {1}"), BodyText, Hint);
		}
	}

	return BodyText;
}

#undef LOCTEXT_NAMESPACE
