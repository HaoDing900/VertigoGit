#include "VTGBPLMInitiateCommandlet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_SwitchName.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "VTGLevelManagerBase.h"
#include "Save/VTGSaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace BPLMInitiate
{
/** The parent BPLM already carries HardInitiateEvent="HardInitiate", so every child is expected to
    own an event by that name - and the ones that do not have been logging an error on every entry. */
const TCHAR* EventName = TEXT("HardInitiate");
/** Preferred first: the name this project already uses for a BPLM's own cached stage. */
const TCHAR* StageNames[] = {TEXT("Stage"), TEXT("CurrentStage")};

/** Every .uasset under this folder; the set of in-level managers is not written down anywhere. */
TArray<FString> Packages(const TCHAR* Root)
{
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *(FPaths::ProjectContentDir() / Root), TEXT("*.uasset"), true, false);
    TArray<FString> Out;
    for (const FString& File : Files)
    {
        FString Package;
        if (FPackageName::TryConvertFilenameToLongPackageName(File, Package) &&
            FPaths::GetBaseFilename(File).StartsWith(TEXT("BPLM")))
            Out.Add(Package);
    }
    Out.Sort();
    return Out;
}

FString PinSummary(UEdGraphPin* Pin, int32 Depth = 1)
{
    if (!Pin) return TEXT("?");
    if (Pin->LinkedTo.IsEmpty())
        return Pin->DefaultValue.IsEmpty() ? TEXT("-") : FString::Printf(TEXT("'%s'"), *Pin->DefaultValue);
    UEdGraphNode* Source = Pin->LinkedTo[0]->GetOwningNode();
    FString Title = Source->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "));
    // One hop further back through pure nodes, so a Switch reads as "Switch on <Get Stage>".
    if (Depth > 0)
        for (UEdGraphPin* Up : Source->Pins)
            if (Up->Direction == EGPD_Input && Up->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
                !Up->LinkedTo.IsEmpty())
                Title += TEXT(" ") + PinSummary(Up, Depth - 1);
    return FString::Printf(TEXT("<%s>"), *Title);
}

void Walk(UEdGraphNode* Node, int32 Depth, TSet<UEdGraphNode*>& Seen, const FString& Prefix)
{
    if (!Node || Depth > 24) return;
    FString Line = Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "));
    for (UEdGraphPin* P : Node->Pins)
        if (P->Direction == EGPD_Input && P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            Line += FString::Printf(TEXT(" %s=%s"), *P->PinName.ToString(), *PinSummary(P));
    UE_LOG(LogTemp, Display, TEXT("%s%s"), *Prefix, *Line);
    if (Seen.Contains(Node)) { UE_LOG(LogTemp, Display, TEXT("%s  (already shown)"), *Prefix); return; }
    Seen.Add(Node);
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
        for (UEdGraphPin* Linked : Pin->LinkedTo)
        {
            const FString Branch = Pin->PinName == FName("then")
                ? Prefix + TEXT("  ")
                : FString::Printf(TEXT("%s  [%s] "), *Prefix, *Pin->PinName.ToString());
            Walk(Linked->GetOwningNode(), Depth + 1, Seen, Branch);
        }
    }
}

// ---- small graph helpers ---------------------------------------------------------------------
UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    if (UEdGraphPin* Found = Node->FindPin(Name)) return Found;
    FString Have;
    for (UEdGraphPin* P : Node->Pins) Have += P->PinName.ToString() + TEXT(" ");
    UE_LOG(LogTemp, Error, TEXT("BPLM no pin '%s' on %s; it has: %s"), Name, *Node->GetName(), *Have);
    checkNoEntry();
    return nullptr;
}
/** The one node an exec pin leads to, or null. */
UEdGraphPin* NextPin(UEdGraphNode* Node, const TCHAR* ExecPin)
{
    UEdGraphPin* Out = Node->FindPin(ExecPin);
    return (!Out || Out->LinkedTo.IsEmpty()) ? nullptr : Out->LinkedTo[0];
}
UEdGraphNode* Next(UEdGraphNode* Node, const TCHAR* ExecPin)
{
    UEdGraphPin* Linked = NextPin(Node, ExecPin);
    return Linked ? Linked->GetOwningNode() : nullptr;
}
UK2Node_Event* StageBeginEvent(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* E = Cast<UK2Node_Event>(Node); E && E->EventReference.GetMemberName() == FName("OnStageBegin"))
            return E;
    return nullptr;
}
/** The two Blueprints converted by hand carry a plain event node, not a UK2Node_CustomEvent, so
    matching on the class alone would have reported them as unconverted and tried to convert them. */
UK2Node_Event* HardInitiateNode(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* Custom = Cast<UK2Node_CustomEvent>(Node); Custom && Custom->CustomFunctionName == FName(EventName))
            return Custom;
        if (auto* E = Cast<UK2Node_Event>(Node); E && E->EventReference.GetMemberName() == FName(EventName))
            return E;
    }
    return nullptr;
}
/** The first Switch on Name reachable along exec from here. */
UK2Node_SwitchName* FindSwitch(UEdGraphNode* From, int32 Depth = 0)
{
    if (!From || Depth > 12) return nullptr;
    if (auto* Switch = Cast<UK2Node_SwitchName>(From)) return Switch;
    for (UEdGraphPin* P : From->Pins)
        if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            for (UEdGraphPin* Linked : P->LinkedTo)
                if (UK2Node_SwitchName* Found = FindSwitch(Linked->GetOwningNode(), Depth + 1)) return Found;
    return nullptr;
}
template <typename TNode> TNode* Add(UEdGraph* Graph, int32 X, int32 Y)
{
    auto* Node = NewObject<TNode>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->NodePosX = X;
    Node->NodePosY = Y;
    return Node;
}
void Backup(UObject* Object)
{
    const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                 FPackageName::GetAssetPackageExtension());
    const FString To = FPaths::ProjectDir() / TEXT("Scripts/BPLMHardInitiate/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
    if (!IFileManager::Get().FileExists(*To))
        check(IFileManager::Get().Copy(*To, *File) == COPY_OK);
}

/** The BPLM's own cached stage: the one the save restores, unlike the GameInstance's. */
FName StageVariable(UBlueprint* Blueprint)
{
    for (const TCHAR* Candidate : StageNames)
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
            if (Var.VarName == FName(Candidate) && Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Name)
                return Var.VarName;
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Name;
    check(FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(StageNames[0]), Type));
    UE_LOG(LogTemp, Display, TEXT("BPLM %s: added a Stage variable (it had none to switch on)"),
           *Blueprint->GetName());
    return FName(StageNames[0]);
}

/** An unhandled stage says so on screen instead of leaving the level silently unbuilt. */
void EnsureDefaultWarning(UEdGraph* Graph, UK2Node_SwitchName* Switch, const FString& Owner)
{
    if (!Switch->bHasDefaultPin)
    {
        Switch->bHasDefaultPin = true;
        Switch->ReconstructNode();
    }
    UEdGraphPin* Default = Pin(Switch, TEXT("Default"));
    if (!Default->LinkedTo.IsEmpty()) return;
    UFunction* Print = UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
    check(Print);
    auto* Warn = Add<UK2Node_CallFunction>(Graph, Switch->NodePosX + 260, Switch->NodePosY - 140);
    Warn->SetFromFunction(Print);
    Warn->AllocateDefaultPins();
    Pin(Warn, TEXT("InString"))->DefaultValue =
        Owner + TEXT(": hard initiate ran with an unhandled stage - the level was not set up");
    Pin(Warn, TEXT("Duration"))->DefaultValue = TEXT("8.000000");
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Default, Pin(Warn, TEXT("execute"))));
}

/** Keyed by title, so re-running refreshes the box instead of stacking a new one. */
void Surround(UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes, const TCHAR* Title, const FString& Body)
{
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Existing = Cast<UEdGraphNode_Comment>(Node); Existing && Existing->NodeComment.StartsWith(Title))
        {
            Existing->NodeComment = FString(Title) + TEXT("\n") + Body;
            return;
        }
    int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
    for (UEdGraphNode* Node : Nodes)
    {
        if (!Node) continue;
        MinX = FMath::Min(MinX, Node->NodePosX);
        MinY = FMath::Min(MinY, Node->NodePosY);
        // Node extents are not stored, so assume a generous box rather than clip the contents.
        MaxX = FMath::Max(MaxX, Node->NodePosX + 420);
        MaxY = FMath::Max(MaxY, Node->NodePosY + 220);
    }
    if (MinX == MAX_int32) return;
    auto* Comment = Add<UEdGraphNode_Comment>(Graph, MinX - 60, MinY - 230);
    Comment->NodeWidth = (MaxX - MinX) + 120;
    Comment->NodeHeight = (MaxY - MinY) + 290;
    Comment->NodeComment = FString(Title) + TEXT("\n") + Body;
    Comment->CommentColor = FLinearColor(1.f, .62f, .2f, .35f);
    Comment->FontSize = 20;
}
}

UVTGBPLMInitiateCommandlet::UVTGBPLMInitiateCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UVTGBPLMInitiateCommandlet::Main(const FString& Params)
{
    using namespace BPLMInitiate;
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    const bool bDump = FParse::Param(*Params, TEXT("Dump"));
    const bool bCaptured = FParse::Param(*Params, TEXT("Captured"));
    UWorld* CaptureWorld = nullptr;
    FString Only;
    FParse::Value(*Params, TEXT("Only="), Only);
    FString Root = TEXT("LevelNSequence/In_Level");
    FParse::Value(*Params, TEXT("Root="), Root);
    int32 Changed = 0, Skipped = 0, Failed = 0;

    for (const FString& Path : Packages(*Root))
    {
        if (!Only.IsEmpty() && !Path.Contains(Only)) continue;
        auto* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
        if (!Blueprint) { UE_LOG(LogTemp, Warning, TEXT("BPLM %s: not a Blueprint"), *Path); continue; }
        // The L4 folder holds two redirectors to Blueprints that live elsewhere; following one would
        // mean editing the same asset twice under two names.
        if (Blueprint->GetOutermost()->GetName() != Path)
        {
            UE_LOG(LogTemp, Display, TEXT("BPLM %s: redirector to %s, skipped"), *Path,
                   *Blueprint->GetOutermost()->GetName());
            continue;
        }
        auto* Defaults = Blueprint->GeneratedClass
                             ? Cast<AVTGLevelManagerBase>(Blueprint->GeneratedClass->GetDefaultObject())
                             : nullptr;
        UE_LOG(LogTemp, Display, TEXT("\n================ %s ================"), *Path);
        UE_LOG(LogTemp, Display, TEXT("  parent=%s levelManagerBase=%d"), *GetNameSafe(Blueprint->ParentClass),
               Defaults != nullptr);
        if (!Defaults) { ++Skipped; continue; }
        UE_LOG(LogTemp, Display, TEXT("  hardInitiateEvent=%s testOverride=%d testStage=%s saveId=%s"),
               *Defaults->HardInitiateEvent.ToString(), Defaults->bOverrideStageForTesting,
               *Defaults->TestStage.ToString(), *Defaults->LevelManagerSaveId.ToString());

        UEdGraph* Graph = nullptr;
        UK2Node_Event* StageBegin = nullptr;
        for (UEdGraph* Page : Blueprint->UbergraphPages)
            if (UK2Node_Event* Found = StageBeginEvent(Page)) { Graph = Page; StageBegin = Found; }
        if (!Graph || !StageBegin)
        {
            UE_LOG(LogTemp, Warning, TEXT("  no On Stage Begin event - nothing to move, skipped"));
            ++Skipped;
            continue;
        }

        if (bApply)
        {
            const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
            Backup(Blueprint);
            const FString Owner = Blueprint->GetName();
            UK2Node_Event* Hard = HardInitiateNode(Graph);

            if (Hard)
            {
                // Already converted (BPLM_BarBack, BPLM_L2StreetBarFight): leave the shape alone and
                // only make sure an unmatched stage is not silent.
                if (UK2Node_SwitchName* Switch = FindSwitch(Next(Hard, TEXT("then"))))
                    EnsureDefaultWarning(Graph, Switch, Owner);
                UE_LOG(LogTemp, Display, TEXT("  already has %s; only checked the Default warning"), EventName);
            }
            else
            {
                const FName Stage = StageVariable(Blueprint);
                UEdGraphPin* StageOut = Pin(StageBegin, TEXT("Stage"));

                // ---- On Stage Begin keeps only "remember the stage" -----------------------------
                UEdGraphNode* First = Next(StageBegin, TEXT("then"));
                auto* Remember = Cast<UK2Node_VariableSet>(First);
                UEdGraphPin* MovedEntry = nullptr;
                if (Remember && Remember->VariableReference.GetMemberName() == Stage)
                {
                    MovedEntry = NextPin(Remember, TEXT("then"));
                    Schema->BreakPinLinks(*Pin(Remember, TEXT("then")), true);
                }
                else
                {
                    // Nothing cached the stage here, so the switch could only ever read the event
                    // pin - which a checkpoint resume never delivers, because it skips this event.
                    MovedEntry = NextPin(StageBegin, TEXT("then"));
                    Schema->BreakPinLinks(*Pin(StageBegin, TEXT("then")), true);
                    Remember = Add<UK2Node_VariableSet>(Graph, StageBegin->NodePosX + 260, StageBegin->NodePosY);
                    Remember->VariableReference.SetSelfMember(Stage);
                    Remember->AllocateDefaultPins();
                    check(Schema->TryCreateConnection(Pin(StageBegin, TEXT("then")), Pin(Remember, TEXT("execute"))));
                    check(Schema->TryCreateConnection(StageOut, Pin(Remember, *Stage.ToString())));
                }
                if (!MovedEntry)
                {
                    UE_LOG(LogTemp, Warning, TEXT("  On Stage Begin drove nothing past the stage - skipped"));
                    ++Skipped;
                    continue;
                }
                UEdGraphNode* MovedHead = MovedEntry->GetOwningNode();

                // ---- and the setup moves onto the hard initiate ----------------------------------
                auto* Custom = Add<UK2Node_CustomEvent>(Graph, MovedHead->NodePosX - 400, MovedHead->NodePosY + 180);
                Custom->CustomFunctionName = FName(EventName);
                Custom->bIsEditable = false;
                Custom->AllocateDefaultPins();
                Hard = Custom;
                check(Schema->TryCreateConnection(Pin(Hard, TEXT("then")), MovedEntry));

                // ---- switching on the variable, not on the event's pin --------------------------
                UK2Node_SwitchName* Switch = FindSwitch(MovedHead);
                if (!Switch)
                {
                    UE_LOG(LogTemp, Warning, TEXT("  no Switch on Name under On Stage Begin - moved the chain "
                                                  "as it was, nothing to re-point"));
                }
                else
                {
                    UEdGraphPin* Selection = Pin(Switch, TEXT("Selection"));
                    TArray<UEdGraphNode*> Stale;
                    for (UEdGraphPin* Linked : Selection->LinkedTo) Stale.AddUnique(Linked->GetOwningNode());
                    Schema->BreakPinLinks(*Selection, true);
                    auto* Lookup = Add<UK2Node_VariableGet>(Graph, Switch->NodePosX - 300, Switch->NodePosY + 140);
                    Lookup->VariableReference.SetSelfMember(Stage);
                    Lookup->AllocateDefaultPins();
                    check(Schema->TryCreateConnection(Pin(Lookup, *Stage.ToString()), Selection));
                    for (UEdGraphNode* Node : Stale)
                        if (Node != StageBegin &&
                            !Node->Pins.ContainsByPredicate([](UEdGraphPin* P) { return !P->LinkedTo.IsEmpty(); }))
                            Graph->RemoveNode(Node);
                    EnsureDefaultWarning(Graph, Switch, Owner);

                    Surround(Graph, {Hard, MovedHead, Switch, Next(Switch, TEXT("Default"))},
                             TEXT("[2026-11-22] 关卡初始化搬到这里 (Hard Initiate)"),
                             FString(TEXT("基类在存档恢复完成之后自动调一次, 三条路径都调:\n"
                                          "正常进图 / 普通读档 / 战斗检查点恢复\n\n"
                                          "原来挂在 On Stage Begin 下面. 恢复战斗检查点的读档会整段跳过\n"
                                          "On Stage Begin (基类 BeginPlay 里的 bEncounterResumePending),\n"
                                          "所以灯光/MPC/NPC 摆位读档之后一个都不跑\n\n"
                                          "1. 不要自己再调 Run Hard Initiate -- 基类调过了, 会跑两次\n"
                                          "2. Switch 读本 BPLM 的 ")) +
                                 Stage.ToString() +
                                 TEXT(" 变量, 不要换成 Get Active Stage\n"
                                      "   GameInstance 的 stage 不会被存档带回来, 读档后是 None\n"
                                      "3. Default 接了 Print String: stage 对不上会喊出来, 不再静默\n\n"
                                      "一次性剧情: 在这条链上插 Branch(Did Enter From Load), False 那边才跑"));
                }
                Surround(Graph, {StageBegin, Remember}, TEXT("[2026-11-22] 这里不再做关卡初始化"),
                         FString(TEXT("只剩 Set ")) + Stage.ToString() +
                             TEXT(". 初始化整条搬去 Event ") + EventName +
                             TEXT(" (见那边的橙框)\n"
                                  "这里以后只放\"只跑一次\"的剧情"));
                ++Changed;
            }

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            FCompilerResultsLog Results;
            FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection,
                                                     &Results);
            UE_LOG(LogTemp, Display, TEXT("  compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
            if (Results.NumErrors || Blueprint->Status == BS_Error)
            {
                UE_LOG(LogTemp, Error, TEXT("  compile FAILED - not saved"));
                ++Failed;
                continue;
            }
            UPackage* Package = Blueprint->GetOutermost();
            Package->FullyLoad();
            ResetLoaders(Package);
            Package->MarkPackageDirty();
            FSavePackageArgs Args;
            Args.TopLevelFlags = RF_Public | RF_Standalone;
            Args.SaveFlags = SAVE_NoError;
            if (!UPackage::SavePackage(Package, Blueprint,
                                       *FPackageName::LongPackageNameToFilename(
                                           Package->GetName(), FPackageName::GetAssetPackageExtension()),
                                       Args))
            {
                UE_LOG(LogTemp, Error, TEXT("  save FAILED"));
                ++Failed;
                continue;
            }
        }

        // ---- what the save actually holds for this BPLM ------------------------------------------
        // Not a reimplementation of the capture rules - it spawns the real class and calls the real
        // CaptureLevelProgress, so what this prints is exactly what a save would contain. The
        // interesting part is what is NOT in the authored variable list: a Do Once node compiles to
        // a hidden bool on the generated class, and whether that bool rides along decides whether a
        // one-shot story beat replays after a load.
        if (bCaptured)
        {
            if (!CaptureWorld)
            {
                const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
                                      .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false)
                                      .ShouldSimulatePhysics(false);
                CaptureWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BPLMCaptureWorld"), nullptr, true,
                                                   ERHIFeatureLevel::Num, &Init);
                check(CaptureWorld);
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(CaptureWorld);
            }
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (auto* Actor = CaptureWorld->SpawnActor<AVTGLevelManagerBase>(Blueprint->GeneratedClass,
                                                                             FTransform::Identity, Spawn))
            {
                FVTGLevelProgressRecord Record;
                Actor->CaptureLevelProgress(Record);
                TArray<FName> Keys;
                Record.Values.GetKeys(Keys);
                Keys.Sort(FNameLexicalLess());
                TSet<FName> Authored;
                for (const FBPVariableDescription& Var : Blueprint->NewVariables) Authored.Add(Var.VarName);
                FString Hidden;
                for (FName Key : Keys)
                    if (!Authored.Contains(Key)) Hidden += Key.ToString() + TEXT(" ");
                UE_LOG(LogTemp, Display, TEXT("  CAPTURED %d values (%d authored); not in the variable list: %s"),
                       Keys.Num(), Authored.Num(), Hidden.IsEmpty() ? TEXT("(none)") : *Hidden);
                Actor->Destroy();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("  CAPTURED: could not spawn it to ask"));
            }
        }

        if (bDump)
        {
            FString Vars;
            for (const FBPVariableDescription& Var : Blueprint->NewVariables)
                Vars += FString::Printf(TEXT("%s(%s) "), *Var.VarName.ToString(), *Var.VarType.PinCategory.ToString());
            UE_LOG(LogTemp, Display, TEXT("  vars: %s"), *Vars);
            for (UEdGraph* Page : Blueprint->UbergraphPages)
            {
                UE_LOG(LogTemp, Display, TEXT("  --- graph %s (%d nodes) ---"), *Page->GetName(), Page->Nodes.Num());
                TSet<UEdGraphNode*> Seen;
                for (UEdGraphNode* Node : Page->Nodes)
                    if (auto* Event = Cast<UK2Node_Event>(Node)) Walk(Event, 0, Seen, TEXT("    "));
            }
        }
        else
        {
            // The shape that matters, in one line per Blueprint.
            UEdGraphNode* After = Next(StageBegin, TEXT("then"));
            UK2Node_Event* Hard = HardInitiateNode(Graph);
            UK2Node_SwitchName* Switch = Hard ? FindSwitch(Next(Hard, TEXT("then"))) : nullptr;
            UE_LOG(LogTemp, Display,
                   TEXT("  RESULT stageBegin -> %s | hardInitiate=%d switch=%d selection=%s default=%d"),
                   After ? *After->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "))
                         : TEXT("(nothing)"),
                   Hard != nullptr, Switch != nullptr,
                   Switch ? *PinSummary(Pin(Switch, TEXT("Selection"))) : TEXT("-"),
                   Switch && Switch->bHasDefaultPin && !Pin(Switch, TEXT("Default"))->LinkedTo.IsEmpty());
        }
    }
    UE_LOG(LogTemp, Display, TEXT("\nBPLM HARD INITIATE changed=%d skipped=%d failed=%d"), Changed, Skipped, Failed);
    return Failed ? 3 : 0;
}
