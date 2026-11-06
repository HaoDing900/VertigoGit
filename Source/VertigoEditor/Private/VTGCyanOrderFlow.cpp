#include "Quests/VTGDialogueStartCondition.h"
#include "Dialogue.h"
#include "DialogueBlueprint.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

int32 VTGRepairCyanOrderFlow(bool bVerifyOnly)
{
    auto* BP = LoadObject<UDialogueBlueprint>(nullptr, TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation"));
    check(BP);
    auto* D = BP->DialogueTemplate;
    UDialogueNode_NPC* End = nullptr;
    UDialogueNode_NPC* Continue = nullptr;
    for (auto* N : D->GetNodes())
    {
        if (N->GetName() == TEXT("DialogueNode_NPC_426")) End = CastChecked<UDialogueNode_NPC>(N);
        if (N->GetName() == TEXT("DialogueNode_NPC_177")) Continue = CastChecked<UDialogueNode_NPC>(N);
    }
    check(End && Continue && End->PlayerReplies.IsEmpty());
    const FName RepeatID(TEXT("L4_BlackMarketInvestigation_Still_MainInvestigation_Identity"));
    auto FindCondition = [&]() -> UVTGDialogueStartCondition*
    {
        for (auto* C : Continue->Conditions) if (auto* Match = Cast<UVTGDialogueStartCondition>(C)) return Match;
        return nullptr;
    };
    if (!bVerifyOnly)
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectDir()/TEXT("Scripts/CyanOrderFlow/Backup")/(BP->GetName()+TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
        auto* Condition = FindCondition();
        if (!Condition)
        {
            Condition = NewObject<UVTGDialogueStartCondition>(Continue, NAME_None, RF_Transactional);
            Continue->Conditions.Add(Condition);
        }
        Condition->StartFromID = RepeatID;
        Condition->bNot = true;
        UEdGraphNode* From = nullptr;
        UEdGraphNode* To = nullptr;
        TArray<UEdGraph*> Graphs;
        BP->GetAllGraphs(Graphs);
        for (auto* G : Graphs) for (UEdGraphNode* N : G->Nodes)
        {
            auto* P = FindFProperty<FObjectPropertyBase>(N->GetClass(), TEXT("DialogueNode"));
            if (!P) continue;
            if (P->GetObjectPropertyValue_InContainer(N) == End) From = N;
            if (P->GetObjectPropertyValue_InContainer(N) == Continue) To = N;
        }
        check(From && To);
        UEdGraphPin* Out = nullptr;
        UEdGraphPin* In = nullptr;
        for (auto* P : From->Pins) if (P->Direction == EGPD_Output) Out = P;
        for (auto* P : To->Pins) if (P->Direction == EGPD_Input) In = P;
        check(Out && In && (Out->LinkedTo.IsEmpty() || Out->LinkedTo.Contains(In)));
        if (!Out->LinkedTo.Contains(In)) check(From->GetGraph()->GetSchema()->TryCreateConnection(Out, In));
        End->NPCReplies = {Continue};
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        check(Results.NumErrors == 0 && BP->Status != BS_Error);
        UE_LOG(LogTemp, Display, TEXT("CYAN_FLOW compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        check(UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args));
    }
    check(End->NPCReplies.Num() == 1 && End->NPCReplies[0] == Continue);
    auto* C = FindCondition();
    check(C && C->bNot && C->StartFromID == RepeatID);
    // Exercise Narrative's real reply-chain builder on a duplicate, leaving the asset unchanged.
    auto* Test = DuplicateObject<UDialogue>(D, GetTransientPackage());
    UDialogueNode_NPC* TestEnd = nullptr;
    UDialogueNode_NPC* TestContinue = nullptr;
    for (auto* N : Test->GetNodes())
    {
        N->OwningDialogue = Test;
        if (N->GetID() == End->GetID()) TestEnd = CastChecked<UDialogueNode_NPC>(N);
        if (N->GetID() == Continue->GetID()) TestContinue = CastChecked<UDialogueNode_NPC>(N);
    }
    check(TestEnd && TestContinue);
    auto* Params = FindFProperty<FStructProperty>(UDialogue::StaticClass(), TEXT("PlayParams"))->ContainerPtrToValuePtr<FDialoguePlayParams>(Test);
    auto* Comp = NewObject<UNarrativeComponent>();
    Params->StartFromID = TEXT("L4_BlackMarketInvestigation_Cyan");
    auto First = TestEnd->GetReplyChain(nullptr, nullptr, Comp);
    check(First.Contains(TestContinue) && First.Num() > 2);
    Params->StartFromID = RepeatID;
    auto Repeat = TestEnd->GetReplyChain(nullptr, nullptr, Comp);
    check(Repeat.Num() == 1 && Repeat[0] == TestEnd);
    UE_LOG(LogTemp, Display, TEXT("CYAN_FLOW PASS: first 426->177 continuation (%d nodes); repeat stops at 426 (%d node)"), First.Num(), Repeat.Num());
    return 0;
}
