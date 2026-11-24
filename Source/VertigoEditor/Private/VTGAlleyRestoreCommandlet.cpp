#include "VTGAlleyRestoreCommandlet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_SwitchName.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "VTGLevelManagerBase.h"

namespace AlleyRestore
{
const TCHAR* BlueprintPath = TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket");
const TCHAR* HardInitiate = TEXT("HardInitiate");
const TCHAR* Stage = TEXT("L4MarketStage_AfterAdult");
/** Puts L4_Block_Alley back where it belongs and hides the ball trigger - the forward path runs it
    from L5_FirstTimeAlley, which only the UponEnterAlley branch reaches. */
const TCHAR* PlaceBarrier = TEXT("L4_InitializeAlley");

/**
 * Which sub-stage branches need what.
 *
 * The barrier and the lock are not symmetric. UponEnterAlley already places the barrier (its chain
 * calls L5_FirstTimeAlley, which calls L4_InitializeAlley), so it only needs the lock.
 * ReadyToLeaveAlley is the branch that DESTROYS the barrier and is immediately followed by the
 * unlock in the forward path, so it needs neither.
 */
struct FBranch
{
    const TCHAR* Case;
    bool bPlaceBarrier;
    bool bBlockSaving;
};
const FBranch Branches[] = {
    {TEXT("UponEnterAlley"), false, true},
    {TEXT("AlleyConflict"), true, true},
    {TEXT("ConflictEnded"), true, true},
};

/** The event that drops the player at the street entrance on a first arrival. */
const TCHAR* StartLocation = TEXT("L4_AftAdtStartLocation");
/** How many nodes at the head of it are "put bodies in places" rather than "set the scene up". */
const int32 PlacementNodes = 2;

UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    if (UEdGraphPin* Found = Node->FindPin(Name)) return Found;
    FString Have;
    for (UEdGraphPin* P : Node->Pins) Have += P->PinName.ToString() + TEXT(" ");
    UE_LOG(LogTemp, Error, TEXT("ALLEY no pin '%s' on %s; it has: %s"), Name, *Node->GetName(), *Have);
    checkNoEntry();
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

UK2Node_CallFunction* AddCall(UEdGraph* Graph, UFunction* Function, int32 X, int32 Y)
{
    auto* Node = Add<UK2Node_CallFunction>(Graph, X, Y);
    Node->SetFromFunction(Function);
    Node->AllocateDefaultPins();
    return Node;
}

/** The hard initiate's inner Switch on SubStage, found by walking the stage switch's own case. */
UK2Node_SwitchName* FindUnder(UEdGraphNode* From, int32 Depth = 0)
{
    if (!From || Depth > 8) return nullptr;
    if (auto* Switch = Cast<UK2Node_SwitchName>(From)) return Switch;
    for (UEdGraphPin* P : From->Pins)
        if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            for (UEdGraphPin* Linked : P->LinkedTo)
                if (UK2Node_SwitchName* Found = FindUnder(Linked->GetOwningNode(), Depth + 1)) return Found;
    return nullptr;
}

/** True when this exec pin's chain already reaches a call to Function, so a re-run adds nothing. */
bool Reaches(UEdGraphPin* Exec, FName Function, int32 Depth = 0)
{
    if (!Exec || Depth > 10) return false;
    for (UEdGraphPin* Linked : Exec->LinkedTo)
    {
        UEdGraphNode* Node = Linked->GetOwningNode();
        if (auto* Call = Cast<UK2Node_CallFunction>(Node);
            Call && Call->FunctionReference.GetMemberName() == Function)
            return true;
        for (UEdGraphPin* P : Node->Pins)
            if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                if (Reaches(P, Function, Depth + 1)) return true;
    }
    return false;
}

UEdGraphNode* Next(UEdGraphNode* Node, const TCHAR* ExecPin = TEXT("then"))
{
    UEdGraphPin* Out = Node->FindPin(ExecPin);
    return (!Out || Out->LinkedTo.IsEmpty()) ? nullptr : Out->LinkedTo[0]->GetOwningNode();
}

/**
 * Stop the street entrance from overwriting where the save left the player.
 *
 * L4_AftAdtStartLocation does two different jobs on one wire: it puts the player and the drone at
 * the entrance, and then it sets the scene up (goons, drone speed, drone following). The first job
 * is only right on a first arrival - on a load, ApplyWorldState has already restored the player's
 * own transform and this runs afterwards, so it throws that away. The second job is right every
 * time. So: branch on DidEnterFromLoad, placement on the False side only, and both sides rejoin
 * before the setup.
 *
 * The drone's placement goes with the player's: IPHAFollowsSa, on the common path, brings it back
 * to wherever the player actually is.
 */
bool GateStartLocation(UBlueprint* Blueprint, UEdGraph* Graph, bool bApply)
{
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    UK2Node_CustomEvent* Event = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Custom = Cast<UK2Node_CustomEvent>(Node); Custom && Custom->CustomFunctionName == FName(StartLocation))
            Event = Custom;
    if (!Event) { UE_LOG(LogTemp, Error, TEXT("ALLEY no %s event"), StartLocation); return false; }

    UEdGraphNode* First = Next(Event);
    if (Cast<UK2Node_IfThenElse>(First))
    {
        UE_LOG(LogTemp, Display, TEXT("ALLEY [%s] already gated"), StartLocation);
        return false;
    }
    // The two placement calls, then whatever they lead into: that node is where the paths rejoin.
    UEdGraphNode* Placement = First;
    UEdGraphNode* Converge = First;
    for (int32 I = 0; I < PlacementNodes; ++I)
    {
        auto* Call = Cast<UK2Node_CallFunction>(Converge);
        if (!Call || Call->FunctionReference.GetMemberName() != FName("K2_SetActorTransform"))
        {
            UE_LOG(LogTemp, Error, TEXT("ALLEY [%s] node %d is %s, not a Set Actor Transform - the event is not the shape this expects, refusing to rewire it"),
                   StartLocation, I, Converge ? *Converge->GetName() : TEXT("(nothing)"));
            return false;
        }
        Converge = Next(Converge);
    }
    if (!Converge) { UE_LOG(LogTemp, Error, TEXT("ALLEY [%s] placement leads nowhere"), StartLocation); return false; }
    UE_LOG(LogTemp, Display, TEXT("ALLEY [%s] gating %d placement node(s); rejoins at %s"), StartLocation,
           PlacementNodes, *Converge->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" ")));
    if (!bApply) return true;

    UFunction* FromLoad = AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("DidEnterFromLoad"));
    check(FromLoad);
    auto* Branch = Add<UK2Node_IfThenElse>(Graph, Event->NodePosX + 220, Event->NodePosY);
    Branch->AllocateDefaultPins();
    auto* Ask = AddCall(Graph, FromLoad, Event->NodePosX + 180, Event->NodePosY + 160);
    check(Schema->TryCreateConnection(Pin(Ask, TEXT("ReturnValue")), Pin(Branch, TEXT("Condition"))));

    Schema->BreakPinLinks(*Pin(Event, TEXT("then")), true);
    check(Schema->TryCreateConnection(Pin(Event, TEXT("then")), Pin(Branch, TEXT("execute"))));
    // False keeps the original wire into the placement; True jumps straight past it.
    check(Schema->TryCreateConnection(Pin(Branch, TEXT("else")), Pin(Placement, TEXT("execute"))));
    check(Schema->TryCreateConnection(Pin(Branch, TEXT("then")), Pin(Converge, TEXT("execute"))));
    Branch->NodeComment = TEXT("[2026-11-24] 读档进来就不要摆玩家了\n"
                               "ApplyWorldState 已经把存档里的玩家位置还原好, 而且跑在 hard initiate 之前,\n"
                               "所以这里再摆一次就是把它盖掉. 第一次从成人店进街才需要摆.\n"
                               "无人机的摆位也一起跳过: 下面的 IPHAFollows Sa 会把它带到玩家身边.");
    Branch->bCommentBubbleVisible = true;
    return true;
}

void Backup(UObject* Object)
{
    const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                 FPackageName::GetAssetPackageExtension());
    const FString To = FPaths::ProjectDir() / TEXT("Scripts/AlleyRestore/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
    if (!IFileManager::Get().FileExists(*To)) check(IFileManager::Get().Copy(*To, *File) == COPY_OK);
}
}

UVTGAlleyRestoreCommandlet::UVTGAlleyRestoreCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UVTGAlleyRestoreCommandlet::Main(const FString& Params)
{
    using namespace AlleyRestore;
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintPath);
    check(Blueprint && Blueprint->GeneratedClass);
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

    UEdGraph* Graph = nullptr;
    UK2Node_Event* Hard = nullptr;
    for (UEdGraph* Page : Blueprint->UbergraphPages)
        for (UEdGraphNode* Node : Page->Nodes)
            if (auto* Custom = Cast<UK2Node_CustomEvent>(Node); Custom && Custom->CustomFunctionName == FName(HardInitiate))
            { Graph = Page; Hard = Custom; }
    check(Graph && Hard);

    UK2Node_SwitchName* StageSwitch = FindUnder(Hard);
    check(StageSwitch);
    UK2Node_SwitchName* SubSwitch = FindUnder(Pin(StageSwitch, Stage)->LinkedTo.IsEmpty()
                                                  ? nullptr
                                                  : Pin(StageSwitch, Stage)->LinkedTo[0]->GetOwningNode());
    check(SubSwitch);

    UFunction* Block = AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("SetManualSaveBlocked"));
    UFunction* Barrier = Blueprint->GeneratedClass->FindFunctionByName(FName(PlaceBarrier));
    check(Block && Barrier);

    int32 Added = 0;
    for (const FBranch& Branch : Branches)
    {
        UEdGraphPin* Case = Pin(SubSwitch, Branch.Case);
        // Insert at the head of the branch, so the barrier is back and saving is shut before any of
        // the scene work that follows can hand control to the player.
        TArray<UFunction*> Wanted;
        if (Branch.bPlaceBarrier && !Reaches(Case, FName(PlaceBarrier))) Wanted.Add(Barrier);
        if (Branch.bBlockSaving && !Reaches(Case, TEXT("SetManualSaveBlocked"))) Wanted.Add(Block);
        if (Wanted.IsEmpty())
        {
            UE_LOG(LogTemp, Display, TEXT("ALLEY [%s] already complete"), Branch.Case);
            continue;
        }
        UE_LOG(LogTemp, Display, TEXT("ALLEY [%s] needs %d node(s)"), Branch.Case, Wanted.Num());
        if (!bApply) { Added += Wanted.Num(); continue; }

        UEdGraphPin* Downstream = Case->LinkedTo.IsEmpty() ? nullptr : Case->LinkedTo[0];
        Schema->BreakPinLinks(*Case, true);
        UEdGraphPin* Tail = Case;
        int32 Row = 0;
        for (UFunction* Function : Wanted)
        {
            auto* Call = AddCall(Graph, Function, SubSwitch->NodePosX + 260, SubSwitch->NodePosY + 420 + Row * 140);
            if (Function == Block) Pin(Call, TEXT("bBlocked"))->DefaultValue = TEXT("true");
            check(Schema->TryCreateConnection(Tail, Pin(Call, TEXT("execute"))));
            Tail = Pin(Call, TEXT("then"));
            ++Added;
            ++Row;
        }
        if (Downstream) check(Schema->TryCreateConnection(Tail, Downstream));
    }

    const bool bGated = GateStartLocation(Blueprint, Graph, bApply);

    if (bApply && (Added || bGated))
    {
        Backup(Blueprint);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        UE_LOG(LogTemp, Display, TEXT("ALLEY compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
        if (Results.NumErrors || Blueprint->Status == BS_Error) return 3;
        UPackage* Package = Blueprint->GetOutermost();
        Package->FullyLoad();
        ResetLoaders(Package);
        Package->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        check(UPackage::SavePackage(Package, Blueprint,
                                    *FPackageName::LongPackageNameToFilename(
                                        Package->GetName(), FPackageName::GetAssetPackageExtension()),
                                    Args));
    }

    // ---- what the branches now say ----------------------------------------------------------------
    for (const FBranch& Branch : Branches)
    {
        UEdGraphPin* Case = Pin(SubSwitch, Branch.Case);
        UE_LOG(LogTemp, Display, TEXT("ALLEY RESULT [%s] barrier=%d blocked=%d"), Branch.Case,
               Reaches(Case, FName(PlaceBarrier)), Reaches(Case, TEXT("SetManualSaveBlocked")));
        if (bApply)
        {
            check(!Branch.bPlaceBarrier || Reaches(Case, FName(PlaceBarrier)));
            check(!Branch.bBlockSaving || Reaches(Case, TEXT("SetManualSaveBlocked")));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("ALLEY %s added=%d"), bApply ? TEXT("APPLIED") : TEXT("would add"), Added);
    return 0;
}
