// Copyright Narrative Tools 2022. 

#include "NarrativeDialogueEditorModule.h"
#include "IDialogueEditor.h"
#include "AssetTypeActions_DialogueBlueprint.h"
#include "AssetTypeActions_DialogueAsset.h"
#include "DialogueGraphEditor.h"
#include "DialogueEditorStyle.h"
#include "PropertyEditorModule.h"
#include "DialogueEditorDetails.h"
#include "DialogueEditorSettings.h"
#include "NarrativeDialogueSettings.h"
#include "EdGraphUtilities.h"
#include "SDialogueGraphNode.h"
#include "DialogueGraphNode.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"
#include <ISettingsCategory.h>
#include "KismetCompiler.h"
#include "Engine/World.h"
#include "DialogueBlueprintCompiler.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphEditor.h"
#include "ToolMenus.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY(LogNarrativeDialogueEditor);

/*
 * Adds a "Jump to Dialogue Node" entry to the right-click menu of Begin Dialogue (and sibling) function-call
 * nodes in any Blueprint graph. It reads the node's literal Dialogue class pin and its Start From ID pin, then
 * opens the referenced dialogue asset and centers on that node - so you can go straight from a Begin Dialogue
 * call to the node its "Start from ID" points at, instead of eyeballing the ID string.
 */
namespace NarrativeDialogueJump
{
	//Functions whose signature is (TSubclassOf<UDialogue> Dialogue, FDialoguePlayParams PlayParams).
	static bool IsDialoguePlayFunction(const FString& FuncName)
	{
		static const TSet<FString> Names = {
			TEXT("BeginDialogue"), TEXT("HasDialogueAvailable"),
			TEXT("SetCurrentDialogue"), TEXT("MakeDialogueInstance")
		};
		return Names.Contains(FuncName);
	}

	//Resolve the dialogue asset from the node's literal "Dialogue" class pin. Returns null if the pin is wired
	//from a variable (nothing to statically resolve) or isn't a dialogue blueprint.
	static UDialogueBlueprint* ResolveDialogueBP(const UK2Node_CallFunction* CallNode)
	{
		if (!CallNode)
		{
			return nullptr;
		}

		const UEdGraphPin* DialoguePin = CallNode->FindPin(TEXT("Dialogue"));
		if (!DialoguePin || DialoguePin->LinkedTo.Num() > 0)
		{
			return nullptr;
		}

		if (UClass* DialogueClass = Cast<UClass>(DialoguePin->DefaultObject))
		{
			return Cast<UDialogueBlueprint>(DialogueClass->ClassGeneratedBy);
		}

		return nullptr;
	}

	//Read the Start From ID, whether the PlayParams struct pin is split (member pin "PlayParams_StartFromID")
	//or collapsed (parse it out of the struct default value).
	static FName ResolveStartFromID(const UK2Node_CallFunction* CallNode)
	{
		if (!CallNode)
		{
			return NAME_None;
		}

		for (const UEdGraphPin* Pin : CallNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input)
			{
				continue;
			}

			if (Pin->PinName.ToString().Contains(TEXT("StartFromID")))
			{
				//If the member pin is wired we can't know the value statically.
				if (Pin->LinkedTo.Num() == 0 && !Pin->DefaultValue.IsEmpty())
				{
					return FName(*Pin->DefaultValue);
				}
				return NAME_None;
			}
		}

		//Collapsed struct pin: default value looks like (StartFromID="Foo",Priority=0)
		if (const UEdGraphPin* ParamsPin = CallNode->FindPin(TEXT("PlayParams")))
		{
			FString Val;
			if (FParse::Value(*ParamsPin->DefaultValue, TEXT("StartFromID="), Val) && !Val.IsEmpty())
			{
				return FName(*Val);
			}
		}

		return NAME_None;
	}

	//Prefix used when Narrative auto-creates the "node started/finished playing" custom events (see
	//FDialogueGraphEditor::OnDialogueNodeDoubleClicked). The node ID is appended after it.
	static const TCHAR* OnPlayedEventPrefix = TEXT("OnDialogueNode Started/Finished Playing - ");

	//Open the dialogue asset's editor and center on the node with the given ID.
	static void OpenAndJump(TWeakObjectPtr<UDialogueBlueprint> WeakBP, FName NodeID)
	{
		UDialogueBlueprint* BP = WeakBP.Get();
		if (!BP || !GEditor)
		{
			return;
		}

		UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (!Sub)
		{
			return;
		}

		Sub->OpenEditorForAsset(BP);

		if (IAssetEditorInstance* Instance = Sub->FindEditorForAsset(BP, true))
		{
			//A UDialogueBlueprint is always edited by an FDialogueGraphEditor.
			FDialogueGraphEditor* DlgEditor = static_cast<FDialogueGraphEditor*>(Instance);
			if (!NodeID.IsNone())
			{
				DlgEditor->JumpToNodeWithID(NodeID);
			}
		}
	}

	//Adds the jump entry to the auto-generated "...Started/Finished Playing - <ID>" custom event nodes inside a
	//DialogueBlueprint's event graph, so you can jump from the event back to the dialogue node that fires it.
	static void PopulateCustomEventMenu(UToolMenu* Menu)
	{
		if (!Menu)
		{
			return;
		}

		UGraphNodeContextMenuContext* Context = Menu->FindContext<UGraphNodeContextMenuContext>();
		if (!Context || !Context->Node)
		{
			return;
		}

		const UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Context->Node);
		if (!EventNode)
		{
			return;
		}

		const FString EventName = EventNode->CustomFunctionName.ToString();
		if (!EventName.StartsWith(OnPlayedEventPrefix))
		{
			return;
		}

		//Only meaningful inside a dialogue blueprint (the node ID resolves against its dialogue graph).
		UDialogueBlueprint* DlgBP = Cast<UDialogueBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(EventNode));
		if (!DlgBP)
		{
			return;
		}

		const FString IDStr = EventName.RightChop(FCString::Strlen(OnPlayedEventPrefix)).TrimStartAndEnd();
		if (IDStr.IsEmpty())
		{
			return;
		}

		const FName NodeID(*IDStr);
		TWeakObjectPtr<UDialogueBlueprint> WeakBP(DlgBP);

		FToolMenuSection& Section = Menu->AddSection("NarrativeDialogueJump", NSLOCTEXT("NarrativeDialogueJump", "SectionHeader", "Narrative"));

		Section.AddMenuEntry(
			"JumpToDialogueNodeFromEvent",
			FText::Format(NSLOCTEXT("NarrativeDialogueJump", "JumpToNode", "Jump to Dialogue Node  \x2192  {0}"), FText::FromString(IDStr)),
			NSLOCTEXT("NarrativeDialogueJump", "JumpToNodeFromEventTooltip", "Center the dialogue graph on the node whose start/finish fires this event."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([WeakBP, NodeID]()
			{
				OpenAndJump(WeakBP, NodeID);
			}))
		);
	}

	static void PopulateMenu(UToolMenu* Menu)
	{
		if (!Menu)
		{
			return;
		}

		UGraphNodeContextMenuContext* Context = Menu->FindContext<UGraphNodeContextMenuContext>();
		if (!Context || !Context->Node)
		{
			return;
		}

		const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Context->Node);
		if (!CallNode)
		{
			return;
		}

		const UFunction* Func = CallNode->GetTargetFunction();
		if (!Func || !IsDialoguePlayFunction(Func->GetName()))
		{
			return;
		}

		UDialogueBlueprint* DlgBP = ResolveDialogueBP(CallNode);
		if (!DlgBP)
		{
			return;
		}

		const FName StartFromID = ResolveStartFromID(CallNode);
		TWeakObjectPtr<UDialogueBlueprint> WeakBP(DlgBP);

		FToolMenuSection& Section = Menu->AddSection("NarrativeDialogueJump", NSLOCTEXT("NarrativeDialogueJump", "SectionHeader", "Narrative"));

		const FText Label = StartFromID.IsNone()
			? NSLOCTEXT("NarrativeDialogueJump", "OpenDialogueAsset", "Open Dialogue Asset")
			: FText::Format(NSLOCTEXT("NarrativeDialogueJump", "JumpToNode", "Jump to Dialogue Node  \x2192  {0}"), FText::FromName(StartFromID));

		Section.AddMenuEntry(
			"JumpToDialogueNode",
			Label,
			NSLOCTEXT("NarrativeDialogueJump", "JumpToNodeTooltip", "Open the referenced Dialogue asset and center on the node this call starts from."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([WeakBP, StartFromID]()
			{
				OpenAndJump(WeakBP, StartFromID);
			}))
		);
	}

	static void Register()
	{
		if (!UToolMenus::IsToolMenuUIEnabled())
		{
			return;
		}

		//K2Node_CallFunction gets its own context menu; extending it means our section only appears on function calls.
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("GraphEditor.GraphNodeContextMenu.K2Node_CallFunction"))
		{
			Menu->AddDynamicSection("NarrativeDialogueJumpDynamic", FNewToolMenuDelegate::CreateStatic(&PopulateMenu));
		}

		//Custom event nodes (the auto-generated "...Started/Finished Playing - <ID>" events) get the reverse jump.
		if (UToolMenu* EventMenu = UToolMenus::Get()->ExtendMenu("GraphEditor.GraphNodeContextMenu.K2Node_CustomEvent"))
		{
			EventMenu->AddDynamicSection("NarrativeDialogueJumpEventDynamic", FNewToolMenuDelegate::CreateStatic(&PopulateCustomEventMenu));
		}
	}
}

const FName FNarrativeDialogueEditorModule::DialogueEditorAppId(TEXT("DialogueEditorApp"));

#define LOCTEXT_NAMESPACE "FNarrativeModule"

uint32 FNarrativeDialogueEditorModule::GameAssetCategory;

class FGraphPanelNodeFactory_DialogueGraph : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node))
		{
			return SNew(SDialogueGraphNode, DialogueNode);
		}
		return NULL;
	}
};

TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory_DialogueGraph;



void FNarrativeDialogueEditorModule::StartupModule()
{
	FDialogueEditorStyle::Initialize();

	RegisterSettings();
	
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	GraphPanelNodeFactory_DialogueGraph = MakeShareable(new FGraphPanelNodeFactory_DialogueGraph());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory_DialogueGraph);

	IAssetTools& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	//Narrative Quest editor has already defined the narrative asset category, so find it 
	GameAssetCategory = AssetToolsModule.FindAdvancedAssetCategory(FName(TEXT("Narrative")));

	TSharedPtr<FAssetTypeActions_DialogueBlueprint> DialogueAssetTypeAction = MakeShareable(new FAssetTypeActions_DialogueBlueprint(GameAssetCategory));
	DialogueAssetTypeActions = DialogueAssetTypeAction;

	//Need to register old asset type actions so people can convert their old DialogueAssets into DialogueBlueprints 
	LegacyDialogueAssetTypeActions = MakeShareable(new FAssetTypeActions_DialogueAsset(GameAssetCategory));

	AssetToolsModule.RegisterAssetTypeActions(DialogueAssetTypeAction.ToSharedRef());
	AssetToolsModule.RegisterAssetTypeActions(LegacyDialogueAssetTypeActions.ToSharedRef());

	FKismetCompilerContext::RegisterCompilerForBP(UDialogueBlueprint::StaticClass(), [](UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
		{
			return MakeShared<FDialogueBlueprintCompilerContext>(CastChecked<UDialogueBlueprint>(InBlueprint), InMessageLog, InCompileOptions);
		});

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&DialogueBlueprintCompiler);

	//Register details panel for quest editor
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("DialogueNode_NPC", FOnGetDetailCustomizationInstance::CreateStatic(&FDialogueEditorDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("DialogueNode_Player", FOnGetDetailCustomizationInstance::CreateStatic(&FDialogueEditorDetails::MakeInstance));
	//PropertyModule.RegisterCustomPropertyTypeLayout("SpeakerSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSpeakerSelectorCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	//Register the "Jump to Dialogue Node" right-click action once tool menus are available.
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&NarrativeDialogueJump::Register));
}


void FNarrativeDialogueEditorModule::ShutdownModule()
{
	//Our menu extension uses static functions from this module, so remove it before the module unloads.
	if (UObjectInitialized() && UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::Get()->RemoveSection("GraphEditor.GraphNodeContextMenu.K2Node_CallFunction", "NarrativeDialogueJumpDynamic");
		UToolMenus::Get()->RemoveSection("GraphEditor.GraphNodeContextMenu.K2Node_CustomEvent", "NarrativeDialogueJumpEventDynamic");
	}

	ToolBarExtensibilityManager.Reset();
	MenuExtensibilityManager.Reset();

	if (UObjectInitialized())
	{
		UnregisterSettings();
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		if (DialogueAssetTypeActions.IsValid())
		{
			AssetToolsModule.UnregisterAssetTypeActions(DialogueAssetTypeActions.ToSharedRef());
		}
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout("SpeakerSelector");

	FDialogueEditorStyle::Shutdown();
}

void FNarrativeDialogueEditorModule::RegisterSettings()
{
	// Registering some settings is just a matter of exposing the default UObject of
		// your desired class, feel free to add here all those settings you want to expose
		// to your LDs or artists.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// Register the settings
		SettingsModule->RegisterSettings("Project", "Plugins", "Narrative Dialogues - Editor",
			LOCTEXT("NarrativeDialogueSettingsName", "Narrative Dialogues - Editor"),
			LOCTEXT("NarrativeDialogueSettingsDescription", "Configuration Settings for the Narrative Dialogue Editor"),
			GetMutableDefault<UDialogueEditorSettings>()
		);

		// Register the runtime settings
		SettingsModule->RegisterSettings("Project", "Plugins", "Narrative Dialogues - Gameplay",
			LOCTEXT("NarrativeRuntimeDialogueSettingsName", "Narrative Dialogues - Gameplay"),
			LOCTEXT("NarrativeRuntimeDialogueSettingsDescription", "Configuration Settings for the Narrative Dialogue Runtime"),
			GetMutableDefault<UNarrativeDialogueSettings>()
		);
	}
}

void FNarrativeDialogueEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Narrative Dialogues - Editor");
		SettingsModule->UnregisterSettings("Project", "Plugins", "Narrative Dialogues - Gameplay");
	}
}

TSharedRef<IDialogueEditor> FNarrativeDialogueEditorModule::CreateDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UDialogueBlueprint* DialogueAsset)
{
	TSharedRef< FDialogueGraphEditor > NewDialogueEditor(new FDialogueGraphEditor());
	NewDialogueEditor->InitDialogueEditor(Mode, InitToolkitHost, DialogueAsset);
	return NewDialogueEditor;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNarrativeDialogueEditorModule, NarrativeDialogueEditor)