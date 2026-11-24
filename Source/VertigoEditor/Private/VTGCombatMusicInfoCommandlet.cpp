#include "VTGCombatMusicInfoCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Audio/VTGCombatMusic.h"
#include "Audio/VTGEnemyAudioComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "UI/VTGInfoInGamePage.h"
#include "UObject/SavePackage.h"
#include "VTGManualLocalization.h"
#include "WidgetBlueprint.h"

namespace VTGCombatInfo
{
const TCHAR* MusicTablePath = TEXT("/Game/SFX/BGM/DT_CombatMusic");
const TCHAR* PageTablePath = TEXT("/Game/Widget/InfoPage/DT_InfoPages");
const TCHAR* LineTablePath = TEXT("/Game/Widget/InfoPage/DT_InfoPageLines");
const TCHAR* PageWidgetFolder = TEXT("/Game/Widget/InfoPage");
const TCHAR* PageWidgetName = TEXT("WBP_InfoInGame");
const TCHAR* PageWidgetPath = TEXT("/Game/Widget/InfoPage/WBP_InfoInGame");
const TCHAR* OldPageWidgetPath = TEXT("/Game/Widget/Tutorials/WBP_Tutorial_Battle");
const TCHAR* FramePath = TEXT("/Game/2DArt/UI/GameOption/InfoBox.InfoBox");

const TCHAR* StreetBPLM = TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight");
const TCHAR* MarketBPLM = TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket");
const TCHAR* SewerBoss = TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer");

/** One namespace, row name plus column as the key: what the manual panel lists these under. */
const TCHAR* TextNamespace = TEXT("VTG.InfoPage");
const TCHAR* SourceCulture = TEXT("en-US");
const TCHAR* TargetCulture = TEXT("zh-Hans");

const FName TrackStreet = TEXT("StreetBarFight");
const FName TrackSewerBoss = TEXT("SewerBoss");
const FName TrackAlley = TEXT("BlackMarketAlley");
const FName CombatPage = TEXT("Combat");

struct FTrackSeed
{
	FName Row;
	const TCHAR* Sound;
	float Volume;
};

/**
 * The three fights that have their own music, and the level that asks for each:
 *   StreetBarFight   - BPLM_L2StreetBarFight, the brawl outside the bar on L2_StreetOutsideBar
 *   SewerBoss        - BP_Boss_SeaMonster_L3Sewer's EnemyAudio, on L_SewerUnderApartment
 *   BlackMarketAlley - BPLM_BlackMarket's alley conflict, in the AfterAdult stage
 *
 * Volumes keep what each fight was mixed at: the street spawned its track at 0.3, and the boss
 * component was set to 0.6. The alley had no music before and takes the street's level.
 */
const FTrackSeed Tracks[] = {
	{TrackStreet, TEXT("/Game/SFX/BGM/CAT-FOX_ALEX/881457__cat-fox_alex__drum-loop_random_200bpm.881457__cat-fox_alex__drum-loop_random_200bpm"), .3f},
	{TrackSewerBoss, TEXT("/Game/SFX/BGM/CAT-FOX_ALEX/881454__cat-fox_alex__drum-loop_deep-4_95bpm.881454__cat-fox_alex__drum-loop_deep-4_95bpm"), .6f},
	{TrackAlley, TEXT("/Game/SFX/BGM/CAT-FOX_ALEX/881507__cat-fox_alex__drumloop_underground-5_90bpm.881507__cat-fox_alex__drumloop_underground-5_90bpm"), .3f},
};

struct FPageSeed
{
	const TCHAR* Column;
	const TCHAR* English;
	const TCHAR* Chinese;
};
/** The combat page keeps what WBP_Tutorial_Battle said, on the pause menu's page grid. */
const FPageSeed CombatPageText[] = {
	{TEXT("Breadcrumb"), TEXT("INFO / COMBAT"), TEXT("提示 / 战斗")},
	{TEXT("Title"), TEXT("Combat Mode"), TEXT("战斗模式")},
	{TEXT("Body"), TEXT(""), TEXT("")},
	{TEXT("DismissLabel"), TEXT("Continue"), TEXT("继续游戏")},
};

struct FLineSeed
{
	const TCHAR* Row;
	int32 Order;
	const TCHAR* Action;
	FKey KeyOverride;
	const TCHAR* English;
	const TCHAR* Chinese;
};
/**
 * Attack and dodge read the player's live binding through their Input Action, so the prompt follows a
 * rebind and turns into the pad glyph by itself. Block has no Input Action of its own yet, so it
 * names its key directly - replace KeyOverride with the action the day one exists.
 */
const FLineSeed CombatLines[] = {
	{TEXT("Combat.Attack"), 0, TEXT("/Game/ThirdPerson/Input/Actions/IA_Fire.IA_Fire"), EKeys::Invalid, TEXT("Attack"), TEXT("攻击")},
	{TEXT("Combat.Block"), 1, nullptr, EKeys::RightMouseButton, TEXT("Block"), TEXT("防御")},
	{TEXT("Combat.Dodge"), 2, TEXT("/Game/ThirdPerson/Input/Actions/IA_Dodge.IA_Dodge"), EKeys::SpaceBar, TEXT("Dodge"), TEXT("闪避")},
};

// ---------------------------------------------------------------------------- asset plumbing

const UEdGraphSchema_K2* S() { return GetDefault<UEdGraphSchema_K2>(); }

UEdGraphPin* P(UEdGraphNode* Node, const TCHAR* Name)
{
	UEdGraphPin* Pin = Node->FindPin(Name);
	checkf(Pin, TEXT("Missing pin %s on %s"), Name, *Node->GetName());
	return Pin;
}

void Link(UEdGraphNode* A, const TCHAR* APin, UEdGraphNode* B, const TCHAR* BPin)
{
	checkf(S()->TryCreateConnection(P(A, APin), P(B, BPin)), TEXT("Link failed %s.%s -> %s.%s"), *A->GetName(), APin, *B->GetName(), BPin);
}

template <class T>
T* AddNode(UEdGraph* Graph, int32 X, int32 Y)
{
	T* Node = NewObject<T>(Graph);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	return Node;
}

UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Class, const TCHAR* Function, int32 X, int32 Y)
{
	auto* Node = AddNode<UK2Node_CallFunction>(Graph, X, Y);
	UFunction* Found = Class->FindFunctionByName(Function);
	checkf(Found, TEXT("Missing function %s on %s"), Function, *Class->GetName());
	Node->SetFromFunction(Found);
	Node->AllocateDefaultPins();
	// A library call in an actor blueprint normally has its world context filled in for it; wire self
	// explicitly when the pin is left visible, rather than compiling with an empty context.
	if (UEdGraphPin* Context = Node->FindPin(TEXT("WorldContextObject")); Context && !Context->bHidden && Context->LinkedTo.Num() == 0)
	{
		auto* Self = AddNode<UK2Node_Self>(Graph, X - 220, Y + 120);
		Self->AllocateDefaultPins();
		S()->TryCreateConnection(Self->FindPinChecked(UEdGraphSchema_K2::PN_Self), Context);
	}
	return Node;
}

void Compile(UBlueprint* Blueprint)
{
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Log;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
	checkf(Log.NumErrors == 0 && Blueprint->Status != BS_Error, TEXT("Compile failed %s (%d errors)"), *Blueprint->GetName(), Log.NumErrors);
	UE_LOG(LogTemp, Display, TEXT("VTG_CMI compile %s errors=%d warnings=%d"), *Blueprint->GetName(), Log.NumErrors, Log.NumWarnings);
}

void Save(UObject* Object)
{
	UPackage* Package = Object->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	checkf(UPackage::SavePackage(Package, Object,
			   *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args),
		TEXT("Save failed %s"), *Package->GetName());
}

void Backup(UObject* Object)
{
	const FString From = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	const FString To = FPaths::ProjectDir() / TEXT("Scripts/CombatMusicInfoPage/Backup") / (Object->GetName() + TEXT(".uasset"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
	if (!IFileManager::Get().FileExists(*To))
	{
		IFileManager::Get().Copy(*To, *From, false);
	}
}

/** A text identity the manual localization panel can see; AsCultureInvariant is skipped by the gather. */
FText Authored(const FString& Key, const TCHAR* Value)
{
	return FText::ChangeKey(TextNamespace, Key, FText::FromString(Value));
}
bool IsTranslatable(const FText& Text)
{
	const auto Key = FTextInspector::GetKey(Text);
	return !Text.IsCultureInvariant() && Key.IsSet() && !Key.GetValue().IsEmpty();
}

UDataTable* LoadOrCreateTable(const TCHAR* Path, UScriptStruct* RowStruct, bool bApply)
{
	auto* Existing = LoadObject<UDataTable>(nullptr, Path);
	if (Existing || !bApply)
	{
		checkf(!Existing || Existing->RowStruct == RowStruct, TEXT("%s has the wrong row struct"), Path);
		return Existing;
	}
	UPackage* Package = CreatePackage(Path);
	check(Package);
	auto* Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(Path), RF_Public | RF_Standalone);
	check(Table);
	Table->RowStruct = RowStruct;
	FAssetRegistryModule::AssetCreated(Table);
	return Table;
}

// ---------------------------------------------------------------------------- the music table

int32 BuildMusicTable(UDataTable* Table, bool bReseed)
{
	check(Table);
	int32 Written = 0;
	for (const FTrackSeed& Seed : Tracks)
	{
		const auto* Current = Table->FindRow<FVTGCombatMusicRow>(Seed.Row, TEXT("Seed"), /*bWarnIfMissing=*/false);
		// A plain apply never clobbers a volume someone tuned in the editor; -Reseed says to put the
		// table back to the baseline here. A row whose source is missing is rewritten either way.
		if (Current && !Current->Music.IsNull() && !bReseed)
		{
			continue;
		}
		FVTGCombatMusicRow Row;
		Row.Music = TSoftObjectPtr<USoundBase>(FSoftObjectPath(Seed.Sound));
		Row.Volume = (Current && !bReseed) ? Current->Volume : Seed.Volume;
		checkf(Row.Music.LoadSynchronous(), TEXT("Track %s: %s did not load"), *Seed.Row.ToString(), Seed.Sound);
		Table->AddRow(Seed.Row, Row);
		++Written;
	}
	if (Written > 0)
	{
		Table->Modify();
		Save(Table);
	}
	return Written;
}

// ---------------------------------------------------------------------------- the info page tables

FString PageKey(const TCHAR* Column) { return FString::Printf(TEXT("%s.%s"), *CombatPage.ToString(), Column); }

int32 BuildPageTable(UDataTable* Table, bool bReseed)
{
	check(Table);
	auto* Current = Table->FindRow<FVTGInfoPageRow>(CombatPage, TEXT("Seed"), /*bWarnIfMissing=*/false);
	const bool bHasIdentity = Current && IsTranslatable(Current->Title);
	if (bHasIdentity && !bReseed)
	{
		return 0;
	}
	FVTGInfoPageRow Row;
	auto Pick = [&](const FPageSeed& Seed, const FText& Existing)
	{
		const FString Wording = (!bReseed && !Existing.IsEmpty() && IsTranslatable(Existing)) ? Existing.ToString() : FString(Seed.English);
		// Empty wording stays empty and untranslatable on purpose: a blank Body hides its line, and a
		// row with no origin text can never be translated anyway.
		return Wording.IsEmpty() ? FText::GetEmpty() : Authored(PageKey(Seed.Column), *Wording);
	};
	Row.Breadcrumb = Pick(CombatPageText[0], Current ? Current->Breadcrumb : FText::GetEmpty());
	Row.Title = Pick(CombatPageText[1], Current ? Current->Title : FText::GetEmpty());
	Row.Body = Pick(CombatPageText[2], Current ? Current->Body : FText::GetEmpty());
	Row.DismissLabel = Pick(CombatPageText[3], Current ? Current->DismissLabel : FText::GetEmpty());
	Table->AddRow(CombatPage, Row);
	Table->Modify();
	Save(Table);
	return 1;
}

int32 BuildLineTable(UDataTable* Table, bool bReseed)
{
	check(Table);
	int32 Written = 0;
	for (const FLineSeed& Seed : CombatLines)
	{
		const FName RowName(Seed.Row);
		const auto* Current = Table->FindRow<FVTGInfoPageLineRow>(RowName, TEXT("Seed"), /*bWarnIfMissing=*/false);
		if (Current && IsTranslatable(Current->Label) && !bReseed)
		{
			continue;
		}
		FVTGInfoPageLineRow Row;
		Row.Page = CombatPage;
		Row.Order = Seed.Order;
		if (Seed.Action)
		{
			Row.Action = TSoftObjectPtr<UInputAction>(FSoftObjectPath(Seed.Action));
			checkf(Row.Action.LoadSynchronous(), TEXT("Line %s: %s did not load"), Seed.Row, Seed.Action);
		}
		Row.KeyOverride = Seed.KeyOverride;
		const FString Wording = (Current && !bReseed && IsTranslatable(Current->Label)) ? Current->Label.ToString() : FString(Seed.English);
		Row.Label = Authored(FString::Printf(TEXT("%s.Label"), Seed.Row), *Wording);
		Table->AddRow(RowName, Row);
		++Written;
	}
	if (Written > 0)
	{
		Table->Modify();
		Save(Table);
	}
	return Written;
}

// ---------------------------------------------------------------------------- WBP_InfoInGame

FSlateFontInfo Font(const TCHAR* Weight, int32 Size)
{
	UFont* Asset = LoadObject<UFont>(nullptr, *(FString(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-")) + Weight + TEXT("_Font")));
	checkf(Asset, TEXT("Missing Noto Sans SC %s"), Weight);
	return FSlateFontInfo(Asset, Size);
}
const FLinearColor Ink(.90f, .95f, .95f, 1.f);
const FLinearColor Accent(.55f, .73f, .76f, 1.f);

/** Moves /Game/Widget/Tutorials/WBP_Tutorial_Battle to /Game/Widget/InfoPage/WBP_InfoInGame. */
UWidgetBlueprint* ResolvePageWidget(bool bApply)
{
	if (auto* Already = LoadObject<UWidgetBlueprint>(nullptr, PageWidgetPath))
	{
		return Already;
	}
	auto* Old = LoadObject<UWidgetBlueprint>(nullptr, OldPageWidgetPath);
	if (!Old || !bApply)
	{
		return Old;
	}
	Backup(Old);
	// Renaming through AssetTools is what fixes up the level manager's hard reference to the old
	// class and leaves a redirector behind; a plain object rename would break that node.
	auto& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	TArray<FAssetRenameData> Renames;
	Renames.Emplace(Old, PageWidgetFolder, PageWidgetName);
	checkf(AssetTools.RenameAssets(Renames), TEXT("Could not rename WBP_Tutorial_Battle"));
	auto* Renamed = LoadObject<UWidgetBlueprint>(nullptr, PageWidgetPath);
	checkf(Renamed, TEXT("WBP_InfoInGame is not at %s after the rename"), PageWidgetPath);
	return Renamed;
}

/** True when the tree has already been rebuilt onto the info-page grid. */
bool PageTreeIsBuilt(UWidgetBlueprint* Widget)
{
	return Widget && Widget->WidgetTree && Widget->WidgetTree->FindWidget(TEXT("InfoFrame")) != nullptr;
}

/**
 * Rebuilds the tree onto the pause menu's sub-page grid.
 *
 * The widgets the blueprint graph is bound to are reused rather than replaced, because that binding
 * is by name: CanvasPanel_25 is what the fade animation drives, and Button_151 is what the graph's
 * OnClicked event - which is what un-pauses the game and gives input back - hangs off. Rebuilding
 * them from scratch would silently drop both.
 */
void BuildPageTree(UWidgetBlueprint* Widget)
{
	UWidgetTree* Tree = Widget->WidgetTree;
	check(Tree);
	auto Find = [Tree](const TCHAR* Name)
	{
		UWidget* Found = Tree->FindWidget(Name);
		checkf(Found, TEXT("WBP_InfoInGame has no widget named %s"), Name);
		return Found;
	};
	auto* Root = CastChecked<UCanvasPanel>(Find(TEXT("CanvasPanel_25")));
	auto* Blur = CastChecked<UBackgroundBlur>(Find(TEXT("BackgroundBlur_82")));
	auto* Dim = CastChecked<UImage>(Find(TEXT("Image_75")));
	auto* Title = CastChecked<UTextBlock>(Find(TEXT("TutorialName")));
	auto* Body = CastChecked<UTextBlock>(Find(TEXT("TextBlock_115")));
	auto* Stack = CastChecked<UVerticalBox>(Find(TEXT("VerticalBox_25")));
	auto* Dismiss = CastChecked<UButton>(Find(TEXT("Button_151")));
	auto* DismissLabel = CastChecked<UTextBlock>(Find(TEXT("TextBlock_135")));

	auto* Frame = LoadObject<UTexture2D>(nullptr, FramePath);
	checkf(Frame, TEXT("Missing %s"), FramePath);

	// Detach everything, then lay it out again. RemoveChild keeps the widget objects, so the two
	// name-bound widgets survive with their names and their bindings.
	Root->ClearChildren();
	Stack->ClearChildren();
	Dismiss->ClearChildren();

	auto Place = [](UCanvasPanel* Panel, UWidget* Child, FVector2D Position, FVector2D Size)
	{
		auto* Slot = Panel->AddChildToCanvas(Child);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	};
	auto Fill = [](UCanvasPanel* Panel, UWidget* Child)
	{
		auto* Slot = Panel->AddChildToCanvas(Child);
		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
		return Slot;
	};

	Fill(Root, Blur);
	Fill(Root, Dim);

	auto* Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("InfoScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	Fill(Root, Scale);

	auto* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InfoSize"));
	Size->SetWidthOverride(940);
	Size->SetHeightOverride(800);
	Scale->SetContent(Size);
	auto* ScaleSlot = CastChecked<UScaleBoxSlot>(Size->Slot);
	ScaleSlot->SetHorizontalAlignment(HAlign_Center);
	ScaleSlot->SetVerticalAlignment(VAlign_Center);

	auto* Panel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InfoPanel"));
	Size->SetContent(Panel);

	auto* FrameImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InfoFrame"));
	FrameImage->SetBrushFromTexture(Frame);
	FrameImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Panel, FrameImage, FVector2D::ZeroVector, FVector2D(940, 800));

	auto* Breadcrumb = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InfoBreadcrumb"));
	Breadcrumb->SetFont(Font(TEXT("Medium"), 16));
	Breadcrumb->SetColorAndOpacity(FSlateColor(Accent));
	Breadcrumb->SetText(FText::GetEmpty());
	Place(Panel, Breadcrumb, FVector2D(100, 94), FVector2D(740, 30));

	// One type scale for every page: the pause sub-pages' 36pt SemiBold title and 1px rule.
	Title->SetFont(Font(TEXT("SemiBold"), 36));
	Title->SetColorAndOpacity(FSlateColor(Ink));
	Title->SetJustification(ETextJustify::Left);
	Title->SetAutoWrapText(true);
	// The old wording carried UMG's auto-generated empty-namespace GUID key, which trips the
	// translation panel's conflict lock. The wording lives in DT_InfoPages now.
	Title->SetText(FText::GetEmpty());
	Place(Panel, Title, FVector2D(100, 142), FVector2D(740, 70));

	auto* Divider = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InfoDivider"));
	Divider->SetBrushColor(FLinearColor(.36f, .65f, .68f, .55f));
	Divider->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Panel, Divider, FVector2D(100, 235), FVector2D(740, 1));

	// Body and key list share one vertical stack, so an empty body collapses and gives its room to
	// the hints instead of leaving a hole.
	Place(Panel, Stack, FVector2D(100, 272), FVector2D(740, 330));
	Body->SetFont(Font(TEXT("Regular"), 20));
	Body->SetColorAndOpacity(FSlateColor(Ink));
	Body->SetJustification(ETextJustify::Left);
	Body->SetAutoWrapText(true);
	Body->SetText(FText::GetEmpty());
	Stack->AddChildToVerticalBox(Body);

	auto* Hints = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InfoHints"));
	if (auto* HintSlot = Cast<UVerticalBoxSlot>(Stack->AddChild(Hints)))
	{
		HintSlot->SetPadding(FMargin(0, 24, 0, 0));
	}

	// The language page's flat button treatment, so the close button matches the pause sub-pages.
	FButtonStyle Style;
	Style.SetNormal(FSlateColorBrush(FLinearColor(.35f, .70f, .74f, .08f)));
	Style.SetHovered(FSlateColorBrush(FLinearColor(.35f, .70f, .74f, .20f)));
	Style.SetPressed(FSlateColorBrush(FLinearColor(.35f, .70f, .74f, .30f)));
	Style.SetDisabled(FSlateNoResource());
	Dismiss->SetStyle(Style);
	DismissLabel->SetFont(Font(TEXT("Medium"), 22));
	DismissLabel->SetColorAndOpacity(FSlateColor(Ink));
	DismissLabel->SetJustification(ETextJustify::Center);
	DismissLabel->SetText(FText::GetEmpty());
	Dismiss->SetContent(DismissLabel);
	if (auto* LabelSlot = Cast<UButtonSlot>(DismissLabel->Slot))
	{
		LabelSlot->SetPadding(FMargin(16, 12));
	}
	Place(Panel, Dismiss, FVector2D(100, 626), FVector2D(240, 62));

	Tree->RootWidget = Root;
	Tree->Modify();
}

void ApplyPageWidget(UWidgetBlueprint* Widget)
{
	check(Widget);
	Backup(Widget);
	BuildPageTree(Widget);
	if (Widget->ParentClass != UVTGInfoInGamePage::StaticClass())
	{
		Widget->ParentClass = UVTGInfoInGamePage::StaticClass();
	}
	Compile(Widget);
	Save(Widget);
}

// ---------------------------------------------------------------------------- fight wiring

UK2Node_CustomEvent* FindEvent(UBlueprint* Blueprint, const TCHAR* Name)
{
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (auto* Event = Cast<UK2Node_CustomEvent>(Node); Event && Event->CustomFunctionName == FName(Name))
			{
				return Event;
			}
		}
	}
	return nullptr;
}

/** Any call to one of the combat-music nodes with this track id, so a second apply is a no-op. */
UK2Node_CallFunction* FindMusicCall(UBlueprint* Blueprint, const TCHAR* Function, FName Track)
{
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			auto* CallNode = Cast<UK2Node_CallFunction>(Node);
			if (!CallNode || CallNode->FunctionReference.GetMemberName() != FName(Function))
			{
				continue;
			}
			if (Track.IsNone())
			{
				return CallNode;
			}
			if (UEdGraphPin* Pin = CallNode->FindPin(TEXT("Track")); Pin && Pin->DefaultValue == Track.ToString())
			{
				return CallNode;
			}
		}
	}
	return nullptr;
}

/** Inserts New into the chain that ExecOut currently drives. */
void SpliceAfter(UEdGraphPin* ExecOut, UK2Node_CallFunction* New)
{
	check(ExecOut && ExecOut->Direction == EGPD_Output);
	TArray<UEdGraphPin*> Downstream = ExecOut->LinkedTo;
	ExecOut->BreakAllPinLinks();
	checkf(S()->TryCreateConnection(ExecOut, P(New, TEXT("execute"))), TEXT("Could not splice %s"), *New->GetName());
	for (UEdGraphPin* Pin : Downstream)
	{
		checkf(S()->TryCreateConnection(P(New, TEXT("then")), Pin), TEXT("Could not reattach downstream of %s"), *New->GetName());
	}
}

UK2Node_CallFunction* MakeStart(UEdGraph* Graph, FName Track, int32 X, int32 Y)
{
	auto* Node = Call(Graph, UVTGCombatMusicLibrary::StaticClass(), TEXT("StartCombatMusic"), X, Y);
	S()->TrySetDefaultValue(*P(Node, TEXT("Track")), Track.ToString());
	Node->NodeComment = FString::Printf(TEXT("VTG combat music: %s (source and volume in DT_CombatMusic)"), *Track.ToString());
	return Node;
}
UK2Node_CallFunction* MakeStop(UEdGraph* Graph, int32 X, int32 Y)
{
	auto* Node = Call(Graph, UVTGCombatMusicLibrary::StaticClass(), TEXT("StopCombatMusic"), X, Y);
	Node->NodeComment = TEXT("VTG combat music: fight over, fade out");
	return Node;
}

/**
 * The street brawl. It used to Spawn Sound 2D straight into the level manager's BGM variable, which
 * neither looped nor ever stopped; the subsystem node does both.
 */
int32 WireStreet(UBlueprint* Blueprint)
{
	int32 Changes = 0;
	if (!FindMusicCall(Blueprint, TEXT("StartCombatMusic"), TrackStreet))
	{
		UK2Node_CallFunction* Spawn = nullptr;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				auto* CallNode = Cast<UK2Node_CallFunction>(Node);
				if (!CallNode || CallNode->FunctionReference.GetMemberName() != TEXT("SpawnSound2D"))
				{
					continue;
				}
				// Identified by the sound it spawns, not by node name: that is what makes this the
				// fight's music and not some other 2D cue the level spawns.
				if (UEdGraphPin* Pin = CallNode->FindPin(TEXT("Sound"));
					Pin && Pin->DefaultObject && Pin->DefaultObject->GetName().Contains(TEXT("Credits_1A_wDrums")))
				{
					Spawn = CallNode;
				}
			}
		}
		checkf(Spawn, TEXT("BPLM_L2StreetBarFight: no Spawn Sound 2D for the fight music - already rewired by hand?"));
		UEdGraph* Graph = Spawn->GetGraph();
		UEdGraphPin* SpawnExec = P(Spawn, TEXT("execute"));
		checkf(SpawnExec->LinkedTo.Num() == 1, TEXT("Spawn Sound 2D has %d exec inputs"), SpawnExec->LinkedTo.Num());
		UEdGraphPin* Upstream = SpawnExec->LinkedTo[0];

		// The Set BGM that caught the spawned component goes with it: nothing else reads that
		// variable after the fade-out above, and the subsystem owns the channel now.
		UEdGraphNode* SetBGM = nullptr;
		if (UEdGraphPin* Then = P(Spawn, TEXT("then")); Then->LinkedTo.Num() == 1)
		{
			if (auto* Setter = Cast<UK2Node_VariableSet>(Then->LinkedTo[0]->GetOwningNode());
				Setter && Setter->VariableReference.GetMemberName() == TEXT("BGM"))
			{
				SetBGM = Setter;
			}
		}

		auto* Start = MakeStart(Graph, TrackStreet, Spawn->NodePosX, Spawn->NodePosY - 180);
		UEdGraphNode* Last = SetBGM ? SetBGM : Cast<UEdGraphNode>(Spawn);
		TArray<UEdGraphPin*> Downstream = P(Last, TEXT("then"))->LinkedTo;
		P(Last, TEXT("then"))->BreakAllPinLinks();
		SpawnExec->BreakAllPinLinks();
		check(S()->TryCreateConnection(Upstream, P(Start, TEXT("execute"))));
		for (UEdGraphPin* Pin : Downstream)
		{
			check(S()->TryCreateConnection(P(Start, TEXT("then")), Pin));
		}
		if (SetBGM)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, SetBGM, true);
		}
		FBlueprintEditorUtils::RemoveNode(Blueprint, Spawn, true);
		++Changes;
	}
	if (!FindMusicCall(Blueprint, TEXT("StopCombatMusic"), NAME_None))
	{
		// The brawl is over the moment the after-fight sequence is asked for, which is the one entry
		// every win path goes through.
		auto* Event = FindEvent(Blueprint, TEXT("PlayAfterFightSequence"));
		checkf(Event, TEXT("BPLM_L2StreetBarFight has no PlayAfterFightSequence event"));
		auto* Stop = MakeStop(Event->GetGraph(), Event->NodePosX + 200, Event->NodePosY - 180);
		SpliceAfter(P(Event, TEXT("then")), Stop);
		++Changes;
	}
	return Changes;
}

/**
 * The black market alley fight, which had no music of its own.
 *
 * The start goes on the far side of the dialogue-exit gate: InitializeAlleyCombat is called from
 * inside a Narrative node event, and its body is deferred until the conversation ends, so wiring the
 * music to the event itself would start it under the last line of dialogue.
 */
int32 WireMarket(UBlueprint* Blueprint)
{
	int32 Changes = 0;
	if (!FindMusicCall(Blueprint, TEXT("StartCombatMusic"), TrackAlley))
	{
		auto* Event = FindEvent(Blueprint, TEXT("InitializeAlleyCombat"));
		checkf(Event, TEXT("BPLM_BlackMarket has no InitializeAlleyCombat event"));
		UEdGraphPin* Then = P(Event, TEXT("then"));
		checkf(Then->LinkedTo.Num() == 1, TEXT("InitializeAlleyCombat drives %d nodes"), Then->LinkedTo.Num());
		auto* Gate = Cast<UK2Node_CallFunction>(Then->LinkedTo[0]->GetOwningNode());
		checkf(Gate && Gate->FunctionReference.GetMemberName() == TEXT("DeferEncounterUntilDialogueEnds"),
			TEXT("InitializeAlleyCombat no longer starts with the dialogue-exit gate"));
		UEdGraphPin* GateThen = P(Gate, TEXT("then"));
		checkf(GateThen->LinkedTo.Num() == 1, TEXT("The alley gate drives %d nodes"), GateThen->LinkedTo.Num());
		auto* Branch = Cast<UK2Node_IfThenElse>(GateThen->LinkedTo[0]->GetOwningNode());
		checkf(Branch, TEXT("The alley gate no longer feeds a branch"));
		// The gate returns true when it deferred, so False is the branch that really starts the fight.
		auto* Start = MakeStart(Branch->GetGraph(), TrackAlley, Branch->NodePosX + 300, Branch->NodePosY - 260);
		SpliceAfter(P(Branch, TEXT("else")), Start);
		++Changes;
	}
	if (!FindMusicCall(Blueprint, TEXT("StopCombatMusic"), NAME_None))
	{
		auto* Event = FindEvent(Blueprint, TEXT("AfterAlleyFightComplete"));
		checkf(Event, TEXT("BPLM_BlackMarket has no AfterAlleyFightComplete event"));
		auto* Stop = MakeStop(Event->GetGraph(), Event->NodePosX + 200, Event->NodePosY - 180);
		SpliceAfter(P(Event, TEXT("then")), Stop);
		++Changes;
	}
	return Changes;
}

/** The sewer boss already owns its own music channel; it only needs to read the shared table. */
int32 WireSewerBoss(UBlueprint* Blueprint)
{
	check(Blueprint->SimpleConstructionScript);
	UVTGEnemyAudioComponent* Audio = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (auto* Candidate = Cast<UVTGEnemyAudioComponent>(Node->ComponentTemplate))
		{
			Audio = Candidate;
		}
	}
	checkf(Audio, TEXT("%s has no VTG Enemy Audio component"), *Blueprint->GetName());
	if (Audio->MusicTrackId == TrackSewerBoss && !Audio->CombatMusic)
	{
		return 0;
	}
	Audio->Modify();
	Audio->MusicTrackId = TrackSewerBoss;
	// An asset in Combat Music wins over the id, so the old cue has to go for the table to be read.
	Audio->CombatMusic = nullptr;
	return 1;
}
}

UVTGCombatMusicInfoCommandlet::UVTGCombatMusicInfoCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVTGCombatMusicInfoCommandlet::Main(const FString& Params)
{
	using namespace VTGCombatInfo;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const bool bReseed = FParse::Param(*Params, TEXT("Reseed"));

	UDataTable* MusicTable = LoadOrCreateTable(MusicTablePath, FVTGCombatMusicRow::StaticStruct(), bApply);
	UDataTable* PageTable = LoadOrCreateTable(PageTablePath, FVTGInfoPageRow::StaticStruct(), bApply);
	UDataTable* LineTable = LoadOrCreateTable(LineTablePath, FVTGInfoPageLineRow::StaticStruct(), bApply);

	if (bApply)
	{
		check(MusicTable && PageTable && LineTable);
		const int32 MusicRows = BuildMusicTable(MusicTable, bReseed);
		const int32 PageRows = BuildPageTable(PageTable, bReseed);
		const int32 LineRows = BuildLineTable(LineTable, bReseed);
		UE_LOG(LogTemp, Display, TEXT("VTG_CMI rows written: music=%d pages=%d lines=%d"), MusicRows, PageRows, LineRows);

		// Point the settings at the tables before anything resolves through them.
		auto* MusicSettings = GetMutableDefault<UVTGCombatMusicSettings>();
		MusicSettings->Tracks = MusicTable;
		MusicSettings->TryUpdateDefaultConfigFile();
		auto* PageSettings = GetMutableDefault<UVTGInfoPageSettings>();
		PageSettings->Pages = PageTable;
		PageSettings->Lines = LineTable;
		PageSettings->TryUpdateDefaultConfigFile();
	}

	UWidgetBlueprint* PageWidget = ResolvePageWidget(bApply);
	if (bApply)
	{
		checkf(PageWidget, TEXT("Neither %s nor %s exists"), PageWidgetPath, OldPageWidgetPath);
		ApplyPageWidget(PageWidget);
	}

	if (bApply)
	{
		int32 Street = 0, Market = 0, Boss = 0;
		auto* StreetBlueprint = LoadObject<UBlueprint>(nullptr, StreetBPLM);
		checkf(StreetBlueprint, TEXT("Missing %s"), StreetBPLM);
		Backup(StreetBlueprint);
		Street = WireStreet(StreetBlueprint);
		if (Street > 0)
		{
			Compile(StreetBlueprint);
			Save(StreetBlueprint);
		}

		auto* MarketBlueprint = LoadObject<UBlueprint>(nullptr, MarketBPLM);
		checkf(MarketBlueprint, TEXT("Missing %s"), MarketBPLM);
		Backup(MarketBlueprint);
		Market = WireMarket(MarketBlueprint);
		if (Market > 0)
		{
			Compile(MarketBlueprint);
			Save(MarketBlueprint);
		}

		auto* BossBlueprint = LoadObject<UBlueprint>(nullptr, SewerBoss);
		checkf(BossBlueprint, TEXT("Missing %s"), SewerBoss);
		Backup(BossBlueprint);
		Boss = WireSewerBoss(BossBlueprint);
		if (Boss > 0)
		{
			Compile(BossBlueprint);
			Save(BossBlueprint);
		}
		UE_LOG(LogTemp, Display, TEXT("VTG_CMI wiring changes: street=%d market=%d boss=%d"), Street, Market, Boss);
	}

	if (bApply)
	{
		// Two steps, as the localization notes require: collect the text identities into
		// Localization/ManualTranslations.json, then compile them into Content/Localization/Vertigo.
		FString Error;
		// The old widget's own labels were collected under its old path. That package is a redirector
		// now, and Compile re-reads the source asset of every entry, so the stale contexts have to go
		// or nothing compiles. The wording itself moved into DT_InfoPages.
		int32 Forgotten = 0;
		checkf(VTGManualLocalization::ForgetAsset(OldPageWidgetPath, Forgotten, Error), TEXT("%s"), *Error);
		UE_LOG(LogTemp, Display, TEXT("VTG_CMI dropped %d stale translation entries from %s"), Forgotten, OldPageWidgetPath);
		checkf(VTGManualLocalization::GatherAssetAndSave(PageTable, Error), TEXT("%s"), *Error);
		checkf(VTGManualLocalization::GatherAssetAndSave(LineTable, Error), TEXT("%s"), *Error);
		auto Approve = [&Error](const FString& Key)
		{
			checkf(VTGManualLocalization::ApproveDisplayText(TextNamespace, Key, Error), TEXT("%s"), *Error);
		};
		auto Translate = [&Error](const FString& Key, const TCHAR* English, const TCHAR* Chinese)
		{
			checkf(VTGManualLocalization::SetTranslation(TextNamespace, Key, SourceCulture, English, Error), TEXT("%s"), *Error);
			checkf(VTGManualLocalization::SetTranslation(TextNamespace, Key, TargetCulture, Chinese, Error), TEXT("%s"), *Error);
		};
		for (const FPageSeed& Seed : CombatPageText)
		{
			if (*Seed.English == TEXT('\0'))
			{
				continue; // an empty column carries no text identity to translate
			}
			Approve(PageKey(Seed.Column));
			Translate(PageKey(Seed.Column), Seed.English, Seed.Chinese);
		}
		for (const FLineSeed& Seed : CombatLines)
		{
			const FString Key = FString::Printf(TEXT("%s.Label"), Seed.Row);
			Approve(Key);
			Translate(Key, Seed.English, Seed.Chinese);
		}
		const bool bCompiled = VTGManualLocalization::Compile(Error);
		UE_LOG(LogTemp, Display, TEXT("VTG_CMI localization compiled=%d %s"), bCompiled, *Error);
		check(bCompiled);
	}

	// ------------------------------------------------------------------ verification (both paths)
	checkf(MusicTable, TEXT("%s is missing - run with -Apply"), MusicTablePath);
	checkf(PageTable, TEXT("%s is missing - run with -Apply"), PageTablePath);
	checkf(LineTable, TEXT("%s is missing - run with -Apply"), LineTablePath);
	for (const FTrackSeed& Seed : Tracks)
	{
		FVTGCombatMusicRow Row;
		checkf(UVTGCombatMusicSubsystem::FindRow(MusicTable, Seed.Row, Row), TEXT("track %s does not resolve through the settings"),
			*Seed.Row.ToString());
		USoundBase* Sound = Row.Music.LoadSynchronous();
		checkf(Sound, TEXT("track %s has no sound"), *Seed.Row.ToString());
		checkf(Row.Volume > 0.f, TEXT("track %s is silent"), *Seed.Row.ToString());
		UE_LOG(LogTemp, Display, TEXT("VTG_CMI track %-18s %s  volume=%.2f fadeIn=%.1f fadeOut=%.1f"), *Seed.Row.ToString(),
			*Sound->GetPathName(), Row.Volume, Row.FadeIn, Row.FadeOut);
	}

	const auto* PageRow = PageTable->FindRow<FVTGInfoPageRow>(CombatPage, TEXT("Verify"), /*bWarnIfMissing=*/false);
	checkf(PageRow, TEXT("%s has no Combat row"), PageTablePath);
	checkf(IsTranslatable(PageRow->Title) && IsTranslatable(PageRow->DismissLabel), TEXT("the Combat page has untranslatable wording"));
	UE_LOG(LogTemp, Display, TEXT("VTG_CMI page Combat  \"%s\" / \"%s\" / dismiss \"%s\""), *PageRow->Breadcrumb.ToString(),
		*PageRow->Title.ToString(), *PageRow->DismissLabel.ToString());
	int32 CombatLineCount = 0;
	for (const auto& Pair : LineTable->GetRowMap())
	{
		const auto* Line = reinterpret_cast<const FVTGInfoPageLineRow*>(Pair.Value);
		if (!Line || Line->Page != CombatPage)
		{
			continue;
		}
		++CombatLineCount;
		checkf(IsTranslatable(Line->Label), TEXT("line %s has no text identity"), *Pair.Key.ToString());
		checkf(!Line->Action.IsNull() || Line->KeyOverride.IsValid(), TEXT("line %s shows no key at all"), *Pair.Key.ToString());
		UE_LOG(LogTemp, Display, TEXT("VTG_CMI line %-16s order=%d action=%s key=%s label=\"%s\""), *Pair.Key.ToString(), Line->Order,
			*Line->Action.ToString(), *Line->KeyOverride.ToString(), *Line->Label.ToString());
	}
	checkf(CombatLineCount == UE_ARRAY_COUNT(CombatLines), TEXT("the Combat page has %d lines, expected %d"), CombatLineCount,
		(int32)UE_ARRAY_COUNT(CombatLines));

	auto* VerifyWidget = LoadObject<UWidgetBlueprint>(nullptr, PageWidgetPath);
	checkf(VerifyWidget, TEXT("%s is missing"), PageWidgetPath);
	checkf(VerifyWidget->ParentClass == UVTGInfoInGamePage::StaticClass(), TEXT("%s is parented to %s"), PageWidgetName,
		*GetNameSafe(VerifyWidget->ParentClass));
	checkf(PageTreeIsBuilt(VerifyWidget), TEXT("%s still has the old tree"), PageWidgetName);
	// The two name-bound widgets are what keep the page dismissable and faded; losing either is silent.
	for (const TCHAR* Name : {TEXT("CanvasPanel_25"), TEXT("Button_151"), TEXT("TextBlock_135"), TEXT("TutorialName"),
			 TEXT("TextBlock_115"), TEXT("InfoFrame"), TEXT("InfoBreadcrumb"), TEXT("InfoHints")})
	{
		checkf(VerifyWidget->WidgetTree->FindWidget(Name), TEXT("%s lost %s"), PageWidgetName, Name);
	}
	auto* VerifyFrame = Cast<UImage>(VerifyWidget->WidgetTree->FindWidget(TEXT("InfoFrame")));
	checkf(VerifyFrame && VerifyFrame->GetBrush().GetResourceObject() == LoadObject<UTexture2D>(nullptr, FramePath),
		TEXT("the page frame is not InfoBox"));
	UE_LOG(LogTemp, Display, TEXT("VTG_CMI %s parent=%s frame=InfoBox"), PageWidgetName, *GetNameSafe(VerifyWidget->ParentClass));

	auto* StreetVerify = LoadObject<UBlueprint>(nullptr, StreetBPLM);
	auto* MarketVerify = LoadObject<UBlueprint>(nullptr, MarketBPLM);
	auto* BossVerify = LoadObject<UBlueprint>(nullptr, SewerBoss);
	checkf(StreetVerify && MarketVerify && BossVerify, TEXT("one of the three fight assets is missing"));
	checkf(FindMusicCall(StreetVerify, TEXT("StartCombatMusic"), TrackStreet), TEXT("the street fight does not start its music"));
	checkf(FindMusicCall(StreetVerify, TEXT("StopCombatMusic"), NAME_None), TEXT("the street fight never stops its music"));
	checkf(FindMusicCall(MarketVerify, TEXT("StartCombatMusic"), TrackAlley), TEXT("the alley fight does not start its music"));
	checkf(FindMusicCall(MarketVerify, TEXT("StopCombatMusic"), NAME_None), TEXT("the alley fight never stops its music"));
	UVTGEnemyAudioComponent* BossAudio = nullptr;
	for (USCS_Node* Node : BossVerify->SimpleConstructionScript->GetAllNodes())
	{
		if (auto* Candidate = Cast<UVTGEnemyAudioComponent>(Node->ComponentTemplate))
		{
			BossAudio = Candidate;
		}
	}
	checkf(BossAudio, TEXT("the sewer boss has no VTG Enemy Audio component"));
	checkf(BossAudio->MusicTrackId == TrackSewerBoss, TEXT("the sewer boss asks for track \"%s\""), *BossAudio->MusicTrackId.ToString());
	checkf(!BossAudio->CombatMusic, TEXT("the sewer boss still has a Combat Music asset, which wins over the table"));

	UE_LOG(LogTemp, Display, TEXT("VTG_CMI PASS (%d tracks, %d combat lines)"), (int32)UE_ARRAY_COUNT(Tracks), CombatLineCount);
	return 0;
}
