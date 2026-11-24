#include "VTGL2HardInitiateCommandlet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_Self.h"
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

namespace L2HardInitiate
{
const TCHAR* BlueprintPath = TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight");
const TCHAR* StageCase = TEXT("Stage_BarStreet_L2");
const TCHAR* ActivateFunction = TEXT("L2_Activate Level Manager");
const TCHAR* CurrentStageVariable = TEXT("L2_CurrentStage=Stage_BarStreet_L2_0");
/** This BPLM caches the stage here, and unlike the GameInstance one it is in the save record. */
const TCHAR* StageVariable = TEXT("Stage");
/** Loud on purpose: a stage that matches no case used to set the level up silently not at all. */
const TCHAR* DefaultWarning = TEXT("BPLM_L2StreetBarFight: hard initiate ran with an unhandled stage - the level was not set up");

UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    for (UEdGraphPin* Candidate : Node->Pins)
        if (Candidate->PinName == FName(Name))
            return Candidate;
    checkf(false, TEXT("no pin %s on %s"), Name, *Node->GetName());
    return nullptr;
}
/** The single node an exec pin leads to, or null when nothing is wired. */
UEdGraphNode* Next(UEdGraphNode* Node, const TCHAR* ExecPin)
{
    UEdGraphPin* Out = Pin(Node, ExecPin);
    return Out->LinkedTo.IsEmpty() ? nullptr : Out->LinkedTo[0]->GetOwningNode();
}
UK2Node_Event* Event(UEdGraph* Graph, const TCHAR* Member)
{
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Candidate = Cast<UK2Node_Event>(Node);
            Candidate && Candidate->EventReference.GetMemberName() == FName(Member))
            return Candidate;
    return nullptr;
}
template <typename TNode> TNode* Add(UEdGraph* Graph, UEdGraphNode* Beside, int32 Column, int32 Row)
{
    auto* Node = NewObject<TNode>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->NodePosX = Beside->NodePosX + Column;
    Node->NodePosY = Beside->NodePosY + Row;
    return Node;
}
UK2Node_CallFunction* AddCall(UEdGraph* Graph, UFunction* Function, UEdGraphNode* Beside, int32 Column, int32 Row)
{
    auto* Node = Add<UK2Node_CallFunction>(Graph, Beside, Column, Row);
    Node->SetFromFunction(Function);
    Node->AllocateDefaultPins();
    return Node;
}
void Backup(UObject* Object)
{
    const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                 FPackageName::GetAssetPackageExtension());
    const FString To = FPaths::ProjectDir() / TEXT("Scripts/L2HardInitiate/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
    if (!IFileManager::Get().FileExists(*To))
        check(IFileManager::Get().Copy(*To, *File) == COPY_OK);
}
}

UVTGL2HardInitiateCommandlet::UVTGL2HardInitiateCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UVTGL2HardInitiateCommandlet::Main(const FString& Params)
{
    using namespace L2HardInitiate;
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintPath);
    check(Blueprint);
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Page : Blueprint->UbergraphPages)
        if (Page->GetName() == TEXT("EventGraph"))
            Graph = Page;
    check(Graph);
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

    UK2Node_Event* StageBegin = Event(Graph, TEXT("OnStageBegin"));
    UK2Node_Event* HardInitiate = Event(Graph, TEXT("HardInitiate"));
    check(StageBegin && HardInitiate);

    // The hard initiate chain is the one that stays; everything the stage event used to drive goes.
    auto* HardSwitch = Cast<UK2Node_SwitchName>(Next(HardInitiate, TEXT("then")));
    check(HardSwitch);

    int32 Removed = 0;
    if (bApply)
    {
        Backup(Blueprint);

        // ---- the stage event stops setting the level up -----------------------------------------
        if (auto* SetStage = Cast<UK2Node_VariableSet>(Next(StageBegin, TEXT("then"))))
        {
            UEdGraphNode* RunHard = Next(SetStage, TEXT("then"));
            auto* Call = Cast<UK2Node_CallFunction>(RunHard);
            if (Call && Call->FunctionReference.GetMemberName() == FName("RunHardInitiate"))
            {
                // Calling it by hand here ran setup before the restore, and the base class calls it
                // again a tick later: the whole chain was running two and three times over.
                auto* BeginSwitch = Cast<UK2Node_SwitchName>(Next(Call, TEXT("then")));
                check(BeginSwitch);
                auto* SetCurrent = Cast<UK2Node_VariableSet>(Next(BeginSwitch, StageCase));
                check(SetCurrent && SetCurrent->VariableReference.GetMemberName() == FName(CurrentStageVariable));
                auto* ActivateFromBegin = Cast<UK2Node_CallFunction>(Next(SetCurrent, TEXT("then")));
                check(ActivateFromBegin && ActivateFromBegin->FunctionReference.GetMemberName() == FName(ActivateFunction));

                // The stage variable is read by another chain in this graph, so Set Stage stays.
                Schema->BreakPinLinks(*Pin(SetStage, TEXT("then")), true);
                Schema->BreakPinLinks(*Pin(SetCurrent, TEXT("execute")), true);
                Schema->BreakPinLinks(*Pin(SetCurrent, TEXT("then")), true);
                for (UEdGraphNode* Doomed : {static_cast<UEdGraphNode*>(Call), static_cast<UEdGraphNode*>(BeginSwitch),
                                             static_cast<UEdGraphNode*>(ActivateFromBegin)})
                {
                    Graph->RemoveNode(Doomed);
                    ++Removed;
                }

                // ---- and the hard initiate takes it over ---------------------------------------
                auto* ActivateFromHard = Cast<UK2Node_CallFunction>(Next(HardSwitch, StageCase));
                check(ActivateFromHard && ActivateFromHard->FunctionReference.GetMemberName() == FName(ActivateFunction));
                // L2_CurrentStage gates five branches inside Activate, so it has to be set on this
                // path too - not left to whatever an earlier session happened to save.
                SetCurrent->NodePosX = HardSwitch->NodePosX + 180;
                SetCurrent->NodePosY = HardSwitch->NodePosY;
                Schema->BreakPinLinks(*Pin(HardSwitch, StageCase), true);
                check(Schema->TryCreateConnection(Pin(HardSwitch, StageCase), Pin(SetCurrent, TEXT("execute"))));
                check(Schema->TryCreateConnection(Pin(SetCurrent, TEXT("then")), Pin(ActivateFromHard, TEXT("execute"))));
            }
        }

        // ---- the stage comes from this Blueprint's own saved variable ----------------------------
        UEdGraphPin* Selection = Pin(HardSwitch, TEXT("Selection"));
        auto IsStageVariable = [](UEdGraphPin* Source)
        {
            auto* Get = Source ? Cast<UK2Node_VariableGet>(Source->GetOwningNode()) : nullptr;
            return Get && Get->VariableReference.GetMemberName() == FName(StageVariable);
        };
        if (Selection->LinkedTo.IsEmpty() || !IsStageVariable(Selection->LinkedTo[0]))
        {
            // GetActiveStage reads GameInstance CurrentStage, and the saves prove it does not survive
            // here: FVTGSlotMeta::Stage is absent from VTG_Slot_*.sav, which tagged serialisation only
            // does for a default value - so a load restores None and no case would ever match. The
            // BPLM's own Stage variable IS in the record (Stage_BarStreet_L2) and RestoreLevelProgress
            // puts it back at the top of BeginPlay, before the hard initiate runs.
            TArray<UEdGraphNode*> Stale;
            for (UEdGraphPin* Linked : Selection->LinkedTo)
            {
                UEdGraphNode* Node = Linked->GetOwningNode();
                Stale.AddUnique(Node);
                for (UEdGraphPin* Feeder : Node->Pins)
                    for (UEdGraphPin* Upstream : Feeder->LinkedTo)
                        if (Upstream->GetOwningNode()->IsA<UK2Node_Self>()) Stale.AddUnique(Upstream->GetOwningNode());
            }
            Schema->BreakPinLinks(*Selection, true);
            auto* Lookup = Add<UK2Node_VariableGet>(Graph, HardSwitch, -300, 120);
            Lookup->VariableReference.SetSelfMember(FName(StageVariable));
            Lookup->AllocateDefaultPins();
            check(Schema->TryCreateConnection(Pin(Lookup, StageVariable), Pin(HardSwitch, TEXT("Selection"))));
            for (UEdGraphNode* Node : Stale)
                if (!Node->Pins.ContainsByPredicate([](UEdGraphPin* P) { return !P->LinkedTo.IsEmpty(); }))
                    Graph->RemoveNode(Node);
        }

        // ---- an unhandled stage says so instead of doing nothing ---------------------------------
        if (!HardSwitch->bHasDefaultPin)
        {
            HardSwitch->bHasDefaultPin = true;
            HardSwitch->ReconstructNode();
        }
        if (UEdGraphPin* Default = Pin(HardSwitch, TEXT("Default")); Default->LinkedTo.IsEmpty())
        {
            UFunction* Print = UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
            check(Print);
            auto* Warn = AddCall(Graph, Print, HardSwitch, 180, 260);
            Pin(Warn, TEXT("InString"))->DefaultValue = DefaultWarning;
            Pin(Warn, TEXT("Duration"))->DefaultValue = TEXT("8.000000");
            check(Schema->TryCreateConnection(Default, Pin(Warn, TEXT("execute"))));
        }
    }

    // ---- let a direct PIE entry carry the stage the bar dialogue would have passed ----------------
    // Only GetActiveStage reads TestStage, only in PIE, and only while nothing has set a stage
    // explicitly - so a real playthrough (which arrives through Open Level with Stage from
    // L2_Bar_NPC) can never see it, and a packaged build cannot either.
    if (bApply)
    {
        if (auto* Defaults = Cast<AVTGLevelManagerBase>(Blueprint->GeneratedClass->GetDefaultObject()))
        {
            if (!Defaults->bOverrideStageForTesting || Defaults->TestStage != FName(StageCase))
            {
                Defaults->Modify();
                Defaults->bOverrideStageForTesting = true;
                Defaults->TestStage = FName(StageCase);
            }
        }
    }

    // ---- say so in the graph itself ---------------------------------------------------------------
    // A rewiring nobody can see is a rewiring nobody can maintain: both ends get a box explaining
    // what moved and why, keyed by the marker so re-running this does not stack duplicates.
    if (bApply)
    {
        auto Surround = [Graph](const TArray<UEdGraphNode*>& Nodes, const TCHAR* Title, const TCHAR* Body)
        {
            // Keyed by title: a re-run refreshes the text in place instead of stacking boxes.
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
                MaxY = FMath::Max(MaxY, Node->NodePosY + 260);
            }
            if (MinX == MAX_int32) return;
            auto* Comment = NewObject<UEdGraphNode_Comment>(Graph);
            Graph->AddNode(Comment, false, false);
            Comment->CreateNewGuid();
            Comment->PostPlacedNewNode();
            Comment->NodePosX = MinX - 60;
            Comment->NodePosY = MinY - 190;
            Comment->NodeWidth = (MaxX - MinX) + 120;
            Comment->NodeHeight = (MaxY - MinY) + 250;
            Comment->NodeComment = FString(Title) + TEXT("\n") + Body;
            Comment->CommentColor = FLinearColor(1.f, .62f, .2f, .35f);
            Comment->FontSize = 20;
        };

        UEdGraphNode* SetStage = Next(StageBegin, TEXT("then"));
        Surround({StageBegin, SetStage},
                 TEXT("[2026-11-22] 这里不再做关卡初始化"),
                 TEXT("原来是: Set Stage -> Run Hard Initiate -> Switch -> Set L2_CurrentStage -> L2_Activate Level Manager\n"
                      "后面四个节点删掉了, 整条初始化搬去 Event Hard Initiate (见那边的橙框)\n\n"
                      "为什么: 恢复战斗检查点的读档会整段跳过 On Stage Begin\n"
                      "(基类 BeginPlay 里的 bEncounterResumePending), 所以挂在这里的\n"
                      "灯光/MPC/NPC 摆位, 读档之后一个都不跑\n\n"
                      "Set Stage 保留: Stage 变量还被 RestrictLeave 那条链读\n"
                      "这里以后只放\"只跑一次\"的剧情"));

        TArray<UEdGraphNode*> HardNodes = {HardInitiate, HardSwitch};
        if (UEdGraphPin* Selection = Pin(HardSwitch, TEXT("Selection")); !Selection->LinkedTo.IsEmpty())
            HardNodes.Add(Selection->LinkedTo[0]->GetOwningNode());
        if (UEdGraphNode* Set = Next(HardSwitch, StageCase))
        {
            HardNodes.Add(Set);
            if (UEdGraphNode* Activate = Next(Set, TEXT("then"))) HardNodes.Add(Activate);
        }
        if (UEdGraphNode* Warn = Next(HardSwitch, TEXT("Default"))) HardNodes.Add(Warn);
        Surround(HardNodes,
                 TEXT("[2026-11-22] 关卡初始化搬到这里 (Hard Initiate)"),
                 TEXT("基类在存档恢复完成之后自动调一次, 三条路径都调:\n"
                      "正常进图 / 普通读档 / 战斗检查点恢复\n\n"
                      "1. 不要自己再调 Run Hard Initiate -- 会重复执行 (之前跑了三次)\n"
                      "2. Switch 读 BPLM 自己的 Stage 变量, 不要换成 Get Active Stage\n"
                      "   GameInstance 的 stage 不会被存档带回来: VTG_Slot_*.sav 的\n"
                      "   FVTGSlotMeta 里没有 Stage 字段 (默认值被跳过), 读档后是 None\n"
                      "   而这个 Stage 变量在 LevelProgress 里, BeginPlay 开头就恢复好了\n"
                      "3. Set L2_CurrentStage 也在这条链上: Activate 内部五个分支要用它\n"
                      "4. Default 接了 Print String: stage 对不上会喊出来, 不再静默\n\n"
                      "一次性剧情: 在这条链上插 Branch(Did Enter From Load), False 那边才跑\n"
                      "手动重跑: 调 Run Hard Initiate"));
    }

    // ---- what the graph now says -----------------------------------------------------------------
    UEdGraphNode* AfterStageBegin = Next(StageBegin, TEXT("then"));
    UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE stage begin -> %s -> %s"),
           AfterStageBegin ? *AfterStageBegin->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("(nothing)"),
           AfterStageBegin && Next(AfterStageBegin, TEXT("then"))
               ? *Next(AfterStageBegin, TEXT("then"))->GetNodeTitle(ENodeTitleType::ListView).ToString()
               : TEXT("(nothing)"));
    UEdGraphNode* Case = Next(HardSwitch, StageCase);
    UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE hard initiate -> switch(%s) -> %s -> %s"),
           Pin(HardSwitch, TEXT("Selection"))->LinkedTo.IsEmpty()
               ? TEXT("?")
               : *Pin(HardSwitch, TEXT("Selection"))->LinkedTo[0]->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(),
           Case ? *Case->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("(nothing)"),
           Case && Next(Case, TEXT("then")) ? *Next(Case, TEXT("then"))->GetNodeTitle(ENodeTitleType::ListView).ToString()
                                            : TEXT("(nothing)"));
    if (auto* Defaults = Cast<AVTGLevelManagerBase>(Blueprint->GeneratedClass->GetDefaultObject()))
    {
        UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE testOverride=%d testStage=%s hardInitiateEvent=%s"),
               Defaults->bOverrideStageForTesting, *Defaults->TestStage.ToString(), *Defaults->HardInitiateEvent.ToString());
        check(Defaults->bOverrideStageForTesting && Defaults->TestStage == FName(StageCase));
        check(Defaults->HardInitiateEvent == FName("HardInitiate"));
    }
    UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE default pin wired=%d removed=%d"),
           HardSwitch->bHasDefaultPin && !Pin(HardSwitch, TEXT("Default"))->LinkedTo.IsEmpty(), Removed);

    // Exactly one Activate call is left, and the stage event no longer reaches it.
    int32 Activates = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Call = Cast<UK2Node_CallFunction>(Node);
            Call && Call->FunctionReference.GetMemberName() == FName(ActivateFunction))
            ++Activates;
    UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE activate call nodes=%d"), Activates);
    check(Activates == 1);
    check(AfterStageBegin && !Next(AfterStageBegin, TEXT("then")));
    check(Case && Cast<UK2Node_VariableSet>(Case));
    check(HardSwitch->bHasDefaultPin && !Pin(HardSwitch, TEXT("Default"))->LinkedTo.IsEmpty());
    {
        UEdGraphPin* Selection = Pin(HardSwitch, TEXT("Selection"));
        check(!Selection->LinkedTo.IsEmpty());
        auto* Source = Cast<UK2Node_VariableGet>(Selection->LinkedTo[0]->GetOwningNode());
        check(Source && Source->VariableReference.GetMemberName() == FName(StageVariable));
    }

    if (!bApply)
        return 0;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
    if (Results.NumErrors || Blueprint->Status == BS_Error)
        return 3;
    UPackage* Package = Blueprint->GetOutermost();
    Package->FullyLoad();
    ResetLoaders(Package);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    check(UPackage::SavePackage(Package, Blueprint,
                                *FPackageName::LongPackageNameToFilename(Package->GetName(),
                                                                         FPackageName::GetAssetPackageExtension()),
                                Args));
    UE_LOG(LogTemp, Display, TEXT("L2 HARD INITIATE PASS"));
    return 0;
}
