#include "VTGCameraLookCommandlet.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

extern bool VTGTestCameraLookPose();
namespace VTGCameraLookEditor
{
template <class T> T *Add(UEdGraph *G, int32 X, int32 Y)
{
    auto *N = NewObject<T>(G);
    G->AddNode(N, false, false);
    N->CreateNewGuid();
    N->PostPlacedNewNode();
    N->NodePosX = X;
    N->NodePosY = Y;
    N->NodeComment = TEXT("VTG Camera Look");
    return N;
}
UEdGraphPin *Output(UEdGraphNode *N)
{
    for (auto *P : N->Pins)
        if (P->Direction == EGPD_Output && P->PinType.PinCategory == TEXT("struct"))
            return P;
    checkNoEntry();
    return nullptr;
}
void Connect(UEdGraph *G, UEdGraphPin *A, UEdGraphPin *B)
{
    check(A && B);
    check(G->GetSchema()->TryCreateConnection(A, B));
}
void Save(UObject *O)
{
    auto *P = O->GetOutermost();
    P->MarkPackageDirty();
    FSavePackageArgs A;
    A.TopLevelFlags = RF_Public | RF_Standalone;
    check(UPackage::SavePackage(
        P, nullptr, *FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension()),
        A));
}
} // namespace VTGCameraLookEditor
UVTGCameraLookCommandlet::UVTGCameraLookCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}
int32 UVTGCameraLookCommandlet::Main(const FString &Params)
{
    using namespace VTGCameraLookEditor;
    auto *BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Characters/Sa/ABP_Sa"));
    check(BP);
    auto *Graph = FBlueprintEditorUtils::FindEventGraph(BP);
    for (UEdGraph *G : BP->FunctionGraphs)
        if (G->GetFName() == TEXT("AnimGraph"))
            Graph = G;
    check(Graph && Graph->GetFName() == TEXT("AnimGraph"));
    if (!FParse::Param(*Params, TEXT("VerifyOnly")))
    {
        check(BP->ParentClass == UAnimInstance::StaticClass() ||
              BP->ParentClass == UVTGCameraLookAnimInstance::StaticClass());
        BP->ParentClass = UVTGCameraLookAnimInstance::StaticClass();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog ParentLog;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &ParentLog);
        check(ParentLog.NumErrors == 0);
        bool Exists = false;
        UAnimGraphNode_Root *Root = nullptr;
        for (UEdGraphNode *N : Graph->Nodes)
        {
            Exists |= N->NodeComment == TEXT("VTG Camera Look");
            if (auto *R = Cast<UAnimGraphNode_Root>(N))
                Root = R;
        }
        check(Root);
        if (!Exists)
        {
            auto *Result = Root->FindPinChecked(TEXT("Result"));
            check(Result->LinkedTo.Num() == 1);
            auto *Previous = Result->LinkedTo[0];
            auto *ToComponent = Add<UAnimGraphNode_LocalToComponentSpace>(Graph, Root->NodePosX - 1000, Root->NodePosY);
            ToComponent->AllocateDefaultPins();
            auto *Neck = Add<UAnimGraphNode_ModifyBone>(Graph, Root->NodePosX - 750, Root->NodePosY);
            auto *Head = Add<UAnimGraphNode_ModifyBone>(Graph, Root->NodePosX - 450, Root->NodePosY);
            int32 I = 0;
            for (auto *N : {Neck, Head})
            {
                N->Node.BoneToModify.BoneName = I == 0 ? TEXT("neck_x") : TEXT("head_x");
                N->Node.RotationMode = BMM_Additive;
                N->Node.RotationSpace = BCS_ComponentSpace;
                N->AllocateDefaultPins();
                auto *Get = Add<UK2Node_VariableGet>(Graph, N->NodePosX - 40, N->NodePosY + 270);
                Get->VariableReference.SetSelfMember(I == 0 ? TEXT("CameraNeckOffset") : TEXT("CameraHeadOffset"));
                Get->AllocateDefaultPins();
                Connect(Graph, Get->GetValuePin(), N->FindPinChecked(TEXT("Rotation")));
                ++I;
            }
            auto *ToLocal = Add<UAnimGraphNode_ComponentToLocalSpace>(Graph, Root->NodePosX - 200, Root->NodePosY);
            ToLocal->AllocateDefaultPins();
            Result->BreakAllPinLinks();
            Connect(Graph, Previous, ToComponent->FindPinChecked(TEXT("LocalPose")));
            Connect(Graph, Output(ToComponent), Neck->FindPinChecked(TEXT("ComponentPose")));
            Connect(Graph, Output(Neck), Head->FindPinChecked(TEXT("ComponentPose")));
            Connect(Graph, Output(Head), ToLocal->FindPinChecked(TEXT("ComponentPose")));
            Connect(Graph, Output(ToLocal), Result);
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog L;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &L);
        check(L.NumErrors == 0);
        Save(BP);
    }
    int32 Fail = 0;
    auto Expect = [&](bool OK, const TCHAR *S)
    {
        Fail += !OK;
        UE_LOG(LogTemp, Display, TEXT("HEAD LOOK %s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), S);
    };
    Expect(BP->GeneratedClass->IsChildOf(UVTGCameraLookAnimInstance::StaticClass()),
           TEXT("existing Sa blueprint inherits camera look data"));
    auto R = UVTGCameraLookAnimInstance::ClampCameraLook(FQuat::Identity, FRotator(20, 45, 0), 60, 30);
    Expect(FMath::IsNearlyEqual(R.Yaw, 45.) && FMath::IsNearlyEqual(R.Pitch, 20.),
           TEXT("camera yaw and pitch follow in correct directions"));
    R = UVTGCameraLookAnimInstance::ClampCameraLook(FRotator(0, 90, 0).Quaternion(), FRotator(0, 90, 0), 60, 30);
    Expect(R.IsNearlyZero(), TEXT("moving body toward camera direction returns look to neutral"));
    R = UVTGCameraLookAnimInstance::ClampCameraLook(FQuat::Identity, FRotator(80, 85, 0), 60, 30);
    Expect(FMath::Abs(R.Pitch) <= 30 && FMath::Abs(R.Yaw) <= 60,
           TEXT("neck rotation remains within anatomical limits"));
    R = UVTGCameraLookAnimInstance::ClampCameraLook(FQuat::Identity, FRotator(0, 179, 0), 60, 30);
    Expect(R.IsNearlyZero(), TEXT("camera behind character cannot snap head through 180 degrees"));
    int32 Nodes = 0;
    for (UEdGraphNode *N : Graph->Nodes)
        if (N->NodeComment == TEXT("VTG Camera Look"))
            ++Nodes;
    Expect(Nodes == 6, TEXT("exactly one camera look chain preserves existing animation graph"));
    if (FParse::Param(*Params, TEXT("Test")))
        Fail += !VTGTestCameraLookPose();
    return Fail ? 1 : 0;
}
