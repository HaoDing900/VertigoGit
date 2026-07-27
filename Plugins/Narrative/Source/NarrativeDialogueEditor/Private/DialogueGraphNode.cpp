// Copyright Narrative Tools 2022. 

#include "DialogueGraphNode.h"
#include "DialogueGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "DialogueGraphSchema.h"
#include "DialogueGraphEditor.h"
#include "DialogueBlueprint.h"
#include "DialogueSM.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "DialogueGraphNode"

FText UDialogueGraphNode::GetNodeText() const
{
	if (DialogueNode)
	{
		const FText BodyText = DialogueNode->Line.Text;

		// Editor-only preview helper: for player options, also surface the Hint Text on the graph node so a
		// hint-only option isn't blank. Shows "Text [Hint]", or just "[Hint]" when Text is empty; falls back
		// to Text alone when there's no hint. This changes the node label ONLY - runtime UI is untouched.
		if (const UDialogueNode_Player* PlayerNode = Cast<UDialogueNode_Player>(DialogueNode))
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

	return FText::GetEmpty();
}

void UDialogueGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	//The node's ID is what a Begin Dialogue node's "Start from ID" pin references. These two actions let you
	//copy that ID onto the clipboard, and jump straight to whatever node an ID on the clipboard refers to.
	const FName NodeID = DialogueNode ? DialogueNode->GetID() : NAME_None;

	FToolMenuSection& Section = Menu->AddSection("DialogueNodeID", LOCTEXT("DialogueNodeIDHeader", "Node ID"));

	Section.AddMenuEntry(
		"CopyNodeID",
		LOCTEXT("CopyNodeID", "Copy Node ID"),
		LOCTEXT("CopyNodeIDTooltip", "Copy this node's ID to the clipboard, ready to paste into a Begin Dialogue \"Start from ID\" pin."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([NodeID]()
			{
				if (!NodeID.IsNone())
				{
					FPlatformApplicationMisc::ClipboardCopy(*NodeID.ToString());
				}
			}),
			FCanExecuteAction::CreateLambda([NodeID]() { return !NodeID.IsNone(); })
		)
	);
}

void UDialogueGraphNode::PostPlacedNewNode()
{
	if (UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(GetGraph()))
	{
		DialogueGraph->NodeAdded(this);
	}
}

void UDialogueGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		UEdGraphPin* OutputPin = GetOutputPin();

		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
		else if (OutputPin != nullptr && GetSchema()->TryCreateConnection(OutputPin, FromPin))
		{
			NodeConnectionListChanged();
		}
	}
}

void UDialogueGraphNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(GetGraph()))
	{
		DialogueGraph->PinRewired(this, Pin);
	}
}


bool UDialogueGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UDialogueGraphSchema::StaticClass());
}

UEdGraphPin* UDialogueGraphNode::GetInputPin(int32 InputIndex /*= 0*/) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* UDialogueGraphNode::GetOutputPin(int32 InputIndex /*= 0*/) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return nullptr;
}

void UDialogueGraphNode::OnStartedOrFinished(UDialogueNode* Node, bool bStarted)
{

}

#undef LOCTEXT_NAMESPACE
