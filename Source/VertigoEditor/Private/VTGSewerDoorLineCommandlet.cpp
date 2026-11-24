#include "VTGSewerDoorLineCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace VTGSewerDoorLine
{
const TCHAR* BlueprintPath = TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt");
const TCHAR* DoorLineNode = TEXT("L3Sewer_Still_DialogueNode_NPC_22");
const TCHAR* SignLineNode = TEXT("L3Sewer_Still_DialogueNode_NPC_28");
const TCHAR* VaultSound = TEXT("445099__lezaarth__vault-2");
const TCHAR* LeverCall = TEXT("L3_OpenRoundDoor");
const TCHAR* StaleComment = TEXT("RoundDoorOpen");
const TCHAR* SignComment = TEXT("Construction Sign Comment");
const TCHAR* LineComment = TEXT("RoundDoorOpen (runs with the door animation and vault sound)");

UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    for (UEdGraphPin* Candidate : Node->Pins)
        if (Candidate->PinName == FName(Name)) return Candidate;
    return nullptr;
}

/** Identify calls by the story node or asset they reference, never by their volatile node name. */
UK2Node_CallFunction* Call(UEdGraph* Graph, const TCHAR* Function, TFunctionRef<bool(UK2Node_CallFunction*)> Matches)
{
    UK2Node_CallFunction* Found = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        auto* Candidate = Cast<UK2Node_CallFunction>(Node);
        if (!Candidate || Candidate->FunctionReference.GetMemberName() != FName(Function) || !Matches(Candidate)) continue;
        checkf(!Found, TEXT("More than one %s call matches"), Function);
        Found = Candidate;
    }
    return Found;
}

UK2Node_CallFunction* DialogueFrom(UEdGraph* Graph, const TCHAR* StartNode)
{
    return Call(Graph, TEXT("BeginDialogue"), [StartNode](UK2Node_CallFunction* Node)
    {
        UEdGraphPin* Start = Pin(Node, TEXT("PlayParams_StartFromID"));
        return Start && Start->DefaultValue == StartNode;
    });
}

/** Keep the moved node out from under its neighbours so the graph stays readable in the editor. */
void PlaceBeside(UEdGraph* Graph, UEdGraphNode* Node, UEdGraphNode* Anchor)
{
    const int32 X = Anchor->NodePosX + 360;
    int32 Y = Anchor->NodePosY + 180;
    for (bool bClear = false; !bClear;)
    {
        bClear = true;
        for (UEdGraphNode* Other : Graph->Nodes)
            if (Other != Node && FMath::Abs(Other->NodePosX - X) < 300 && FMath::Abs(Other->NodePosY - Y) < 160)
            {
                Y = Other->NodePosY + 180;
                bClear = false;
                break;
            }
    }
    Node->NodePosX = X;
    Node->NodePosY = Y;
}

int32 Run(UBlueprint* Blueprint, UEdGraph* Graph, bool bVerifyOnly)
{
    UK2Node_CallFunction* Line = DialogueFrom(Graph, DoorLineNode);
    UK2Node_CallFunction* Vault = Call(Graph, TEXT("PlaySoundAtLocation"), [](UK2Node_CallFunction* Node)
    {
        UEdGraphPin* Sound = Pin(Node, TEXT("Sound"));
        return Sound && Sound->DefaultObject && Sound->DefaultObject->GetName().Contains(VaultSound);
    });
    if (!Line || !Vault)
    {
        UE_LOG(LogTemp, Error, TEXT("SEWER DOOR LINE missing nodes: line=%d vault=%d"), Line != nullptr, Vault != nullptr);
        return 10;
    }
    UEdGraphPin* LineExec = Pin(Line, TEXT("execute"));
    UEdGraphPin* VaultThen = Pin(Vault, TEXT("then"));
    if (!LineExec || !VaultThen) return 11;

    const bool bWired = LineExec->LinkedTo.Num() == 1 && LineExec->LinkedTo[0] == VaultThen;
    for (UEdGraphPin* Driver : LineExec->LinkedTo)
        UE_LOG(LogTemp, Display, TEXT("SEWER DOOR LINE driver: %s | %s"), *Driver->GetOwningNode()->GetName(),
            *Driver->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
    UE_LOG(LogTemp, Display, TEXT("SEWER DOOR LINE state: inputs=%d vault outputs=%d at door=%d"),
        LineExec->LinkedTo.Num(), VaultThen->LinkedTo.Num(), bWired);
    UE_LOG(LogTemp, Display, TEXT("SEWER DOOR LINE line comment: %s"), *Line->NodeComment);

    if (bVerifyOnly) return bWired ? 0 : 12;
    if (!bWired)
    {
        // The lever call returns as soon as its chain suspends on the 1 s delay, so the line fired a second early.
        UK2Node_CallFunction* Lever = Call(Graph, LeverCall, [](UK2Node_CallFunction*) { return true; });
        const bool bFromLever = LineExec->LinkedTo.Num() == 1 && Lever && LineExec->LinkedTo[0] == Pin(Lever, TEXT("then"));
        if (!LineExec->LinkedTo.IsEmpty() && !bFromLever)
        {
            UE_LOG(LogTemp, Error, TEXT("SEWER DOOR LINE is driven by something unexpected; leaving it alone"));
            return 13;
        }
        if (!VaultThen->LinkedTo.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("SEWER DOOR LINE vault sound already continues elsewhere; leaving it alone"));
            return 14;
        }
        const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
        Schema->BreakPinLinks(*LineExec, true);
        if (!Schema->TryCreateConnection(VaultThen, LineExec)) return 15;
        PlaceBeside(Graph, Line, Vault);
        if (Line->NodeComment == StaleComment || Line->NodeComment.IsEmpty()) Line->NodeComment = LineComment;
    }
    // The construction-sign comment inherited the door line's label and reads as the wrong beat.
    if (UK2Node_CallFunction* Sign = DialogueFrom(Graph, SignLineNode); Sign && Sign->NodeComment == StaleComment)
        Sign->NodeComment = SignComment;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return 0;
}
}

UVTGSewerDoorLineCommandlet::UVTGSewerDoorLineCommandlet() { IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UVTGSewerDoorLineCommandlet::Main(const FString& Params)
{
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, VTGSewerDoorLine::BlueprintPath);
    if (!Blueprint) return 1;
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Page : Blueprint->UbergraphPages)
        if (Page->GetName() == TEXT("EventGraph")) Graph = Page;
    if (!Graph) return 2;

    const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
    if (const int32 Result = VTGSewerDoorLine::Run(Blueprint, Graph, bVerifyOnly); Result != 0) return Result;
    if (bVerifyOnly)
    {
        UE_LOG(LogTemp, Display, TEXT("SEWER DOOR LINE verified"));
        return 0;
    }

    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    UE_LOG(LogTemp, Display, TEXT("SEWER DOOR LINE compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
    if (Results.NumErrors || Blueprint->Status == BS_Error) return 3;

    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Save)) return 5;
    UE_LOG(LogTemp, Display, TEXT("SEWER DOOR LINE saved %s"), *Filename);
    return 0;
}
