// Copyright Narrative Tools 2022. 

#include "DialogueEditorDetails.h"
#include "DetailLayoutBuilder.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "DialogueBlueprint.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SComboBox.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "DialogueEditorDetails"

TSharedRef<IDetailCustomization> FDialogueEditorDetails::MakeInstance()
{
	return MakeShareable(new FDialogueEditorDetails);
}


FText FDialogueEditorDetails::GetSpeakerText() const
{
	//When argument changes auto-update the description
	if (LayoutBuilder)
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		LayoutBuilder->GetObjectsBeingCustomized(EditedObjects);

		if (EditedObjects.IsValidIndex(0))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(EditedObjects[0].Get()))
			{
				return FText::FromName(NPCNode->GetSpeakerID());
			}
		}
	}

	return LOCTEXT("SpeakerText", "None");
}

TSharedRef<SWidget> FDialogueEditorDetails::MakeWidgetForOption(TSharedPtr<FText> InOption)
{
	return SNew(STextBlock).Text(*InOption);
}

void FDialogueEditorDetails::OnSelectionChanged(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	//When argument changes auto-update the description
	if (LayoutBuilder)
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		LayoutBuilder->GetObjectsBeingCustomized(EditedObjects);

		if (EditedObjects.IsValidIndex(0))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(EditedObjects[0].Get()))
			{
				NPCNode->SetSpeakerID(FName(NewSelection->ToString()));
			}
		}
	}
}

FReply FDialogueEditorDetails::SetTransformsFromActorSelection()
{
	//TArray<TWeakObjectPtr<UObject>> EditedObjects;
	//LayoutBuilder->GetObjectsBeingCustomized(EditedObjects);

	//if (EditedObjects.IsValidIndex(0))
	//{
	//	if (UDialogue* Dialogue = Cast<UDialogue>(EditedObjects[0].Get()))
	//	{
	//		

	//		LayoutBuilder->ForceRefreshDetails();
	//	}
	//}

	return FReply::Handled();
}

void FDialogueEditorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	LayoutBuilder = &DetailLayout;

	//Hide the unused fields that live in the node's "Default" category. These are Blueprint variables on the node BP,
	//so we resolve them against the node's actual (Blueprint-generated) class, otherwise HideProperty can't find them.
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsToHideFrom;
		DetailLayout.GetObjectsBeingCustomized(ObjectsToHideFrom);

		if (ObjectsToHideFrom.IsValidIndex(0) && ObjectsToHideFrom[0].IsValid())
		{
			UClass* NodeClass = ObjectsToHideFrom[0]->GetClass();

			static const TArray<FName> PropsToHide =
			{
				FName("Icon"), FName("IconHover"), FName("IconClicked"),
				FName("CharacterImg"), FName("ShowPortrait"), FName("ShowUI")
			};

			for (const FName& PropName : PropsToHide)
			{
				TSharedRef<IPropertyHandle> Handle = DetailLayout.GetProperty(PropName, NodeClass);
				if (Handle->IsValidHandle())
				{
					DetailLayout.HideProperty(Handle);
				}
			}
		}
	}

	//Control the order categories appear in within the dialogue node details panel.
	//Lower number = higher up. Only categories that actually exist get touched, so no empty headers are created.
	DetailLayout.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& CategoryMap)
	{
		static const TMap<FName, int32> CategorySortOrder =
		{
			{FName("Default"),                        0},
			{FName("Details"),                        10},
			{FName("Details - Player Dialogue Node"), 20},
			{FName("Details - NPC Dialogue Node"),    20},
			{FName("Dialogue Line"),                  30},
			{FName("Speaker Details"),                40},
			{FName("Events & Conditions"),            50},
			{FName("Details - Dialogue Node"),        60},
			{FName("Details - Dialogue Editor"),      70},
			{FName("Legacy"),                         80},
			{FName("Debug"),                          90},
		};

		for (const TPair<FName, IDetailCategoryBuilder*>& Pair : CategoryMap)
		{
			if (const int32* SortOrder = CategorySortOrder.Find(Pair.Key))
			{
				Pair.Value->SetSortOrder(*SortOrder);
			}
		}
	});

	TArray<TWeakObjectPtr<UObject>> EditedObjects;
	DetailLayout.GetObjectsBeingCustomized(EditedObjects);

	IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Details");

	FText GroupLabel(LOCTEXT("DetailsGroup", "Details"));

	if (EditedObjects.Num() > 0 && EditedObjects.IsValidIndex(0))
	{
		if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(EditedObjects[0].Get()))
		{
			if (!NPCNode->OwningDialogue)
			{
				return;
			}
		}
	}
}


//TSharedRef<IPropertyTypeCustomization> FSpeakerSelectorCustomization::MakeInstance()
//{
//	return MakeShareable(new FSpeakerSelectorCustomization());
//}
//
//void FSpeakerSelectorCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
//{
//	HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
//
//	TArray<UObject*> OuterObjects;
//
//	PropertyHandle->GetOuterObjects(OuterObjects);
//
//	UDialogueNode* DialogueNode = nullptr;
//
//	for (auto& Obj : OuterObjects)
//	{
//		if (UDialogueNode* NodeObj = Cast<UDialogueNode>(Obj))
//		{
//			DialogueNode = NodeObj;
//			break;
//		}
//	}
//
//	if (UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(DialogueNode->OwningDialogue->GetOuter()))
//	{
//		Dialogue = Cast<UDialogue>(DialogueBP->GeneratedClass->GetDefaultObject());
//	}
//
//	if(Dialogue)
//	{
//		for (const auto& Speaker : Dialogue->Speakers)
//		{
//			SpeakersList.Add(MakeShareable(new FName(Speaker.SpeakerID)));
//
//			//if (Speaker.SpeakerID == NPCNode->SpeakerID)
//			//{
//			//	SelectedItem = SpeakersList.Last();
//			//}
//		}
//
//		//Add a button to make the quest designer more simplified 
//		ComboBox = SNew(SComboBox<TSharedPtr<FName>>)
//			.OptionsSource(&SpeakersList)
//			.OnSelectionChanged(this, &FSpeakerSelectorCustomization::OnSelectionChanged)
//			.InitiallySelectedItem(SelectedItem)
//			.OnGenerateWidget_Lambda([](TSharedPtr<FText> Option)
//				{
//					return SNew(STextBlock)
//						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
//						.Text(*Option);
//				})
//			[
//				SNew(STextBlock)
//				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
//					.Text(this, &FDialogueEditorDetails::GetSpeakerText)
//			];
//
//
//	}
//}
//
//void FSpeakerSelectorCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
//{
//}
//
////FText FSpeakerSelectorCustomization::GetSpeakerText() const
////{
////
////}
////
////TSharedRef<SWidget> FSpeakerSelectorCustomization::MakeWidgetForOption(TSharedPtr<FText> InOption)
////{
////
////}
//
//void FSpeakerSelectorCustomization::OnSelectionChanged(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
//{
//
//}
//
//void FSpeakerSelectorCustomization::UpdateProperty()
//{
//
//}

#undef LOCTEXT_NAMESPACE
