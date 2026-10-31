//  Copyright Narrative Tools 2022.


#include "QuestNodeUserWidget.h"


void UQuestNodeUserWidget::InitializeFromNode(class UQuestNode* InNode, class UQuest* InQuest)
{
	if (InNode)
	{
		Node = InNode;
		Quest = InQuest;

		OnNodeInitialized(InNode, InQuest);

		// Default Quest editor widget binds this legacy description directly to Node.Description.
		// Collapse its presentation without clearing saved quest outcome descriptions.
		if (auto* DescriptionWidget = GetWidgetFromName(TEXT("TextBlock_74")))
			DescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
