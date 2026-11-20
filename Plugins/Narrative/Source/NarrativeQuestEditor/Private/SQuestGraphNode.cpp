// Copyright Narrative Tools 2022. 

#include "SQuestGraphNode.h"
#include "QuestGraphNode.h"
#include "QuestGraphNode_PersistentTasks.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Editor.h"
#include "QuestGraph.h"
#include "SGraphPanel.h"
#include "ScopedTransaction.h"
#include "QuestSM.h"
#include "QuestEditorStyle.h"
#include "Quest.h"
#include "QuestGraphNode_State.h"
#include "NarrativeEvent.h"
#include "QuestEditorSettings.h"
#include <Blueprint/UserWidget.h>
#include "Components/VerticalBox.h"
#include <Slate/SObjectWidget.h>
#include "QuestNodeUserWidget.h"
#include "QuestBlueprint.h"
#include "NarrativeQuestDetailsExtension.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"

#define LOCTEXT_NAMESPACE "SQuestGraphNode"

void SQuestGraphNode::Construct(const FArguments& InArgs, UQuestGraphNode* InNode)
{
	GraphNode = InNode;

	UpdateGraphNode();

	if (OwnerGraphPanelPtr.IsValid())
	{
		OwnerGraphPanelPtr.Pin()->AttachGraphEvents(SharedThis(this));
	}

}

void SQuestGraphNode::CreatePinWidgets()
{
	UQuestGraphNode* QuestGraphNode = CastChecked<UQuestGraphNode>(GraphNode);

	for (int32 PinIdx = 0; PinIdx < QuestGraphNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* MyPin = QuestGraphNode->Pins[PinIdx];
		if (!MyPin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = SNew(SGraphPin, MyPin);

			AddPin(NewPin.ToSharedRef());
		}
	}
}

void SQuestGraphNode::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SGraphNode::Tick(Geometry, Time, Delta);
    if (auto Panel = GetOwnerPanel(); Panel && LocalizationBadgeWidget)
    {
        const float Zoom = Panel->GetZoomAmount();
        if (Zoom <= KINDA_SMALL_NUMBER) return;
        const float Scale = FMath::Clamp(FMath::Pow(1.f / Zoom, .8f), 1.f, 3.f);
        if (!FMath::IsNearlyEqual(Scale, LastAppliedBadgeScale))
        {
            LastAppliedBadgeScale = Scale;
            LocalizationBadgeWidget->SetRenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(Scale))));
        }
    }
}

EVisibility SQuestGraphNode::GetConnectedEventVis() const
{
    const auto* Node = Cast<UQuestGraphNode>(GraphNode);
    if (!Node || !Node->QuestNode || Node->QuestNode->OnEnteredFuncName.IsNone()) return EVisibility::Collapsed;
    const auto* Blueprint = Node->GetTypedOuter<UQuestBlueprint>();
    if (!Blueprint) return EVisibility::Collapsed;
    for (const UEdGraph* EventGraph : Blueprint->UbergraphPages)
    {
        if (!EventGraph) continue;
        for (const UEdGraphNode* EventNode : EventGraph->Nodes)
        {
            const auto* Event = Cast<UK2Node_CustomEvent>(EventNode);
            if (!Event || Event->CustomFunctionName != Node->QuestNode->OnEnteredFuncName) continue;
            for (const UEdGraphPin* Pin : Event->Pins)
                if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.Num())
                    return EVisibility::Visible;
        }
    }
    return EVisibility::Collapsed;
}

void SQuestGraphNode::UpdateGraphNode()
{

	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();
	QuestNodeWidgetRef.Reset();

	UQuestGraphNode* QuestGraphNode = Cast<UQuestGraphNode>(GraphNode);
	UQuestNode* QuestNode = QuestGraphNode ? QuestGraphNode->QuestNode : nullptr;
    TSharedPtr<SWidget> LocalizationBadge;
    if (GetNarrativeQuestLocalizationBadge().IsBound()) LocalizationBadge = GetNarrativeQuestLocalizationBadge().Execute(QuestNode);
    LocalizationBadgeWidget = LocalizationBadge;
    LastAppliedBadgeScale = 0.f;
    if (!LocalizationBadge) LocalizationBadge = SNullWidget::NullWidget;
    LocalizationBadge->SetRenderTransformPivot(FVector2D(1.f, 0.f));
    static const FSlateRoundedBoxBrush ConnectedEventBrush(FLinearColor(.02f, .35f, 1.f), FVector2f(10.f, 10.f));
    const TSharedRef<SWidget> ConnectedEventIndicator = SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
        .Visibility(this, &SQuestGraphNode::GetConnectedEventVis)
        .ToolTipText(LOCTEXT("ConnectedEventTip", "This quest node's Blueprint event has a connected execution output."))
        [SNew(SImage).Image(&ConnectedEventBrush)];


	UQuestNodeUserWidget* QuestNodeWidgetInstance = nullptr;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	// In a function where you want to get the reference to the UUserWidget asset
	// Load the UUserWidget asset using SoftObjectPtr::LoadSynchronous
	if (const UQuestEditorSettings* QuestSettings = GetDefault<UQuestEditorSettings>())
	{
		UClass* WidgetClass = QuestSettings->DefaultQuestWidgetClass.LoadSynchronous();

		// Spawn an instance of the UUserWidget class
		QuestNodeWidgetInstance = CreateWidget<UQuestNodeUserWidget>(World, WidgetClass);
	}

	if (QuestNodeWidgetInstance && QuestNode)
	{
		if (UQuestBlueprint* QuestBP = Cast<UQuestBlueprint>(GraphNode->GetGraph()->GetOuter()))
		{
			if (UQuest* OwningQuest = Cast<UQuest>(QuestBP->GeneratedClass->GetDefaultObject()))
			{
				QuestNodeWidgetInstance->InitializeFromNode(QuestNode, OwningQuest);
				QuestNodeWidgetRef = QuestNodeWidgetInstance->TakeWidget();

				if (QuestNodeWidgetInstance->RightPinBox && QuestNodeWidgetInstance->LeftPinBox)
				{
					//Tell the C++ code to autopopulate our UMG blueprint widgets pin boxes! 
					RightNodeBox = StaticCastSharedRef<SVerticalBox>(QuestNodeWidgetInstance->RightPinBox->TakeWidget());
					LeftNodeBox = StaticCastSharedRef<SVerticalBox>(QuestNodeWidgetInstance->LeftPinBox->TakeWidget());
				}
				else
				{
					//Didnt have pin boxes, clear the Quest node ref to force node to use old style 
					QuestNodeWidgetRef.Reset();
				}
			}
		}
	}

	//Fancy new Quest UMG based nodes!
	if (QuestNodeWidgetRef.IsValid())
	{

		this->GetOrAddSlot(ENodeZone::Center)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
			[
				QuestNodeWidgetRef.ToSharedRef()
			]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(4)[LocalizationBadge.ToSharedRef()]
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(8)[ConnectedEventIndicator]
			];
	}
	else // Old style yucky slate ones
	{


	FSlateRenderTransform QuantityRender;
	QuantityRender.SetTranslation(FVector2D(0.f, -30.f));

	FSlateRenderTransform CurrentStateIconRender;
	CurrentStateIconRender.SetTranslation(FVector2D(0.f, -96.f));


	auto SizeBox = SNew(SBox);
	SizeBox->SetMinDesiredWidth(250.f);
	SizeBox->SetMinDesiredHeight(60.f);

	this->GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("BTEditor.Graph.BTNode.Body"))
		.BorderBackgroundColor(this, &SQuestGraphNode::GetBorderColor)
		[
			SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(4)[LocalizationBadge.ToSharedRef()]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(8)[ConnectedEventIndicator]
			+ SOverlay::Slot()
		[
			SizeBox
		]
	+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(5.f)
		[

			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredHeight(10.f)
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
			]
	+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.FillWidth(1.f)
		[
	SNew(SVerticalBox)
	+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SQuestGraphNode::GetNodeTitleText)
		.ColorAndOpacity(this, &SQuestGraphNode::GetNodeTitleColor)
		.Justification(ETextJustify::Center)
		.Clipping(EWidgetClipping::Inherit)
		.TextStyle(FAppStyle::Get(), TEXT("PhysicsAssetEditor.Tools.Font"))
		]
	+ SVerticalBox::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SQuestGraphNode::GetNodeText)
		.Justification(ETextJustify::Center)
		.WrapTextAt(200.f)
		]
	+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		.FillHeight(1.f)
		[
			SNew(STextBlock)
			.Text(this, &SQuestGraphNode::GetEventsText)
		.Visibility(this, &SQuestGraphNode::GetEventsVis)
		.Justification(ETextJustify::Center)
		.AutoWrapText(true)
		.TextStyle(FAppStyle::Get(), TEXT("PhysicsAssetEditor.Tools.Font"))
		.ColorAndOpacity(this, &SQuestGraphNode::GetEventsColor)
		]
		]
	+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredHeight(10.f)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
		]
		]
	]
	]
];
	}

	CreatePinWidgets();
}

TSharedPtr<SToolTip> SQuestGraphNode::GetComplexTooltip()
{
	return nullptr;
}

void SQuestGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));


	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		LeftNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			.Padding(5.f, 0.f)
			[
				PinToAdd
			];
		InputPins.Add(PinToAdd);
	}
	else
	{
		RightNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			[
				PinToAdd
			];
		OutputPins.Add(PinToAdd);
	}

	

}

FText SQuestGraphNode::GetNodeText() const
{
	UQuestGraphNode* Node = CastChecked<UQuestGraphNode>(GraphNode);

	if (Node->IsA<UQuestGraphNode_PersistentTasks>())
	{
		return FText::FromString("Connecting a new up to this node is the same as connecting every state in the quest to that node.");
	}

	return FText::GetEmpty();
}


FText SQuestGraphNode::GetEventsText() const
{
	FString EvtText = "";

	if (UQuestGraphNode* QuestGraphNode = Cast<UQuestGraphNode>(GraphNode))
	{
		if (QuestGraphNode->QuestNode)
		{
			if (QuestGraphNode->QuestNode->Events.Num())
			{
				int32 Idx = 0;
				for (auto& Evt : QuestGraphNode->QuestNode->Events)
				{
					if (Evt)
					{
						//Only append newline if we have more than one Evtition
						if (Idx == 0)
						{
							EvtText += Evt->GetGraphDisplayText();
						}
						else
						{
							EvtText += LINE_TERMINATOR + Evt->GetGraphDisplayText();
						}

						Idx++;
					}
				}
			}
		}
	}

	return FText::FromString(EvtText);
}

FText SQuestGraphNode::GetNodeTitleText() const
{
	UQuestGraphNode* Node = CastChecked<UQuestGraphNode>(GraphNode);

	if (Node->IsA<UQuestGraphNode_PersistentTasks>())
	{
		return FText::FromString("Any State");
	}

	return (Node && Node->QuestNode) ? Node->QuestNode->GetNodeTitle() : FText::GetEmpty();
}

FText SQuestGraphNode::GetNodeQuantityText() const
{
	//UQuestGraphNode* Node = CastChecked<UQuestGraphNode>(GraphNode);

	//if (Node->IsA<UQuestGraphNode_State>())
	//{
	//	if (UQuestState* StateNode = Cast<UQuestState>(Node->QuestNode))
	//	{
	//		if (UQuest* OwningQuest = StateNode->GetOwningQuest())
	//		{
	//			if (OwningQuest->GetCurrentState() == StateNode)
	//			{
	//				return LOCTEXT("StateCurrentNode", "CURRENT STATE");
	//			}
	//		}
	//	}
	//}
	return FText::GetEmpty();
}


EVisibility SQuestGraphNode::GetEventsVis() const
{
	return GetEventsText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SQuestGraphNode::GetNodeTextVisibility() const
{
	if (GetNodeText().IsEmpty()) return EVisibility::Collapsed;
	TSharedPtr<SGraphPanel> MyOwnerPanel = GetOwnerPanel();
	return (!MyOwnerPanel.IsValid() || MyOwnerPanel->GetCurrentLOD() > EGraphRenderingLOD::LowDetail) ? EVisibility::Visible : EVisibility::Collapsed;
}


FSlateColor SQuestGraphNode::GetNodeTitleColor() const
{
	return  FSlateColor(FColor::White);
}


FSlateColor SQuestGraphNode::GetEventsColor() const
{
	return  FSlateColor(FColor::White);
}

FSlateColor SQuestGraphNode::GetBorderColor() const
{

	if (UQuestGraphNode* QuestGraphNode = Cast<UQuestGraphNode>(GraphNode))
	{
		return QuestGraphNode->GetGraphNodeColor();
	}

	return FLinearColor::Gray;
}

void SQuestGraphNodePin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	this->SetCursor(EMouseCursor::Default);

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SQuestGraphNodePin::GetPinBorder)
		.BorderBackgroundColor(this, &SQuestGraphNodePin::GetPinColor)
		.Cursor(EMouseCursor::Crosshairs)
		.Padding(FMargin(10.0f))
	);
}

FSlateColor SQuestGraphNodePin::GetPinColor() const
{
	return FSlateColor(FLinearColor::White);
}

TSharedRef<SWidget> SQuestGraphNodePin::GetDefaultValueWidget()
{
	//Todo find out why this is here
	return SNew(STextBlock);
}

const FSlateBrush* SQuestGraphNodePin::GetPinBorder() const
{
	return FAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Body"));
}

#undef LOCTEXT_NAMESPACE
