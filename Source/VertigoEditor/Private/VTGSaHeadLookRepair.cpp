#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

// Run with -run=VTGJawMovement -SaHeadLookAfterSlot [-VerifyOnly].
int32 VTGMoveSaHeadLookAfterSlot(const FString& Params)
{
    auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Characters/Sa/ABP_Sa"));
    check(BP);
    UEdGraph* Graph = nullptr;
    for (UEdGraph* G : BP->FunctionGraphs) if (G->GetFName() == TEXT("AnimGraph")) Graph = G;
    check(Graph);
    auto Find = [&](const TCHAR* Name) -> UEdGraphNode*
    {
        for (UEdGraphNode* N : Graph->Nodes) if (N->GetFName() == Name) return N;
        checkNoEntry(); return nullptr;
    };
    auto* ToComponent = Find(TEXT("AnimGraphNode_LocalToComponentSpace_1"));
    auto* Neck = CastChecked<UAnimGraphNode_ModifyBone>(Find(TEXT("AnimGraphNode_ModifyBone_0")));
    auto* ToLocal = Find(TEXT("AnimGraphNode_ComponentToLocalSpace_2"));
    auto* OldSlot = Find(TEXT("AnimGraphNode_Slot_3"));
    auto* DefaultSlot = CastChecked<UAnimGraphNode_Slot>(Find(TEXT("AnimGraphNode_Slot_4")));
    auto* Jaw = Find(TEXT("AnimGraphNode_NarrativeJaw_0"));
    check(Neck->Node.BoneToModify.BoneName == TEXT("neck_x"));
    check(DefaultSlot->Node.SlotName == TEXT("DefaultSlot"));
    auto Pin = [](UEdGraphNode* N, const TCHAR* Name) { return N->FindPinChecked(Name); };
    auto Linked = [](UEdGraphPin* A, UEdGraphPin* B) { return A->LinkedTo.Num() == 1 && A->LinkedTo[0] == B; };
    check(Linked(Pin(Neck, TEXT("ComponentPose")), Pin(ToComponent, TEXT("ComponentPose"))));
    check(Linked(Pin(ToLocal, TEXT("ComponentPose")), Pin(Neck, TEXT("Pose"))));
    check(Pin(Neck, TEXT("Rotation"))->LinkedTo.Num() == 1);
    int32 OriginalNeckCount = 0;
    for (UEdGraphNode* N : Graph->Nodes) if (auto* Bone = Cast<UAnimGraphNode_ModifyBone>(N))
        if (Bone->Node.BoneToModify.BoneName == TEXT("neck_x")) ++OriginalNeckCount;
    const bool AlreadyMoved = Linked(Pin(ToComponent, TEXT("LocalPose")), Pin(DefaultSlot, TEXT("Pose")))
        && Linked(Pin(Jaw, TEXT("SourcePose")), Pin(ToLocal, TEXT("Pose")));
    if (!AlreadyMoved)
    {
        if (FParse::Param(*Params, TEXT("VerifyOnly"))) return 1;
        // Reject unexpected graphs rather than replacing any unrelated user edits.
        check(Linked(Pin(OldSlot, TEXT("Source")), Pin(ToLocal, TEXT("Pose"))));
        check(Linked(Pin(Jaw, TEXT("SourcePose")), Pin(DefaultSlot, TEXT("Pose"))));
        auto* Input = Pin(ToComponent, TEXT("LocalPose"));
        check(Input->LinkedTo.Num() == 1);
        auto* Previous = Input->LinkedTo[0];
        check(Previous->GetOwningNode()->GetFName() == TEXT("AnimGraphNode_LayeredBoneBlend_6"));
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectDir() / TEXT("Scripts/BlackMarketLookDiagnosis/ABP_Sa.before-headlook.uasset");
        if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
        BP->Modify(); Graph->Modify();
        for (auto* N : {ToComponent, static_cast<UEdGraphNode*>(Neck), ToLocal, OldSlot, static_cast<UEdGraphNode*>(DefaultSlot), Jaw}) N->Modify();
        Input->BreakAllPinLinks();
        Pin(OldSlot, TEXT("Source"))->BreakAllPinLinks();
        Pin(Jaw, TEXT("SourcePose"))->BreakAllPinLinks();
        auto Connect = [&](UEdGraphPin* A, UEdGraphPin* B) { check(Graph->GetSchema()->TryCreateConnection(A, B)); };
        Connect(Previous, Pin(OldSlot, TEXT("Source")));
        Connect(Pin(DefaultSlot, TEXT("Pose")), Input);
        Connect(Pin(ToLocal, TEXT("Pose")), Pin(Jaw, TEXT("SourcePose")));
        // Keep the original neck node and its rotation/settings; move its complete space-conversion chain.
        ToComponent->NodePosX = DefaultSlot->NodePosX + 300;
        ToComponent->NodePosY = DefaultSlot->NodePosY + 250;
        Neck->NodePosX = ToComponent->NodePosX + 250;
        Neck->NodePosY = ToComponent->NodePosY;
        ToLocal->NodePosX = Neck->NodePosX + 300;
        ToLocal->NodePosY = ToComponent->NodePosY;
        Neck->NodeComment = TEXT("Head look after DefaultSlot (including Sequencer animation)");
        Neck->bCommentBubbleVisible = true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Log);
    if (Log.NumErrors) return 2;
    check(Linked(Pin(ToComponent, TEXT("LocalPose")), Pin(DefaultSlot, TEXT("Pose"))));
    check(Linked(Pin(Jaw, TEXT("SourcePose")), Pin(ToLocal, TEXT("Pose"))));
    int32 NeckCount = 0;
    for (UEdGraphNode* N : Graph->Nodes) if (auto* Bone = Cast<UAnimGraphNode_ModifyBone>(N))
        if (Bone->Node.BoneToModify.BoneName == TEXT("neck_x")) ++NeckCount;
    check(NeckCount == OriginalNeckCount);
    if (!AlreadyMoved)
    {
        auto* Package = BP->GetOutermost();
        Package->FullyLoad(); ResetLoaders(Package); BP->MarkPackageDirty();
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Package, BP, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args)) return 3;
    }
    UE_LOG(LogTemp, Display, TEXT("SA HEAD LOOK PASS: DefaultSlot -> LocalToComponent -> neck_x -> ComponentToLocal -> NarrativeJaw; original neck controller count preserved=%d; compile errors=%d warnings=%d"), NeckCount, Log.NumErrors, Log.NumWarnings);
    return 0;
}
