#include "K2Node_DynamicCast.h"
#include "K2Node_Self.h"

namespace TabFocusRepair
{
int32 Run(bool VerifyOnly)
{
    auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcher"));
    if (!BP) return 1;
    UEdGraph* ActiveGraph = nullptr;
    UEdGraph* KeyGraph = nullptr;
    for (UEdGraph* Graph : BP->FunctionGraphs)
    {
        if (Graph->GetName() == TEXT("SetActiveTab")) ActiveGraph = Graph;
        if (Graph->GetName() == TEXT("OnKeyDown")) KeyGraph = Graph;
    }
    if (!ActiveGraph || !KeyGraph) return 2;
    UK2Node_DynamicCast* MailCast = nullptr;
    UK2Node_CallFunction* InputCall = nullptr;
    for (UEdGraphNode* Node : ActiveGraph->Nodes)
        if (auto* CastNode = Cast<UK2Node_DynamicCast>(Node))
            if (CastNode->TargetType && CastNode->TargetType->GetName() == TEXT("WB_TabWidget_Mail_C")) MailCast = CastNode;
    for (UEdGraphNode* Node : KeyGraph->Nodes)
        if (auto* Call = Cast<UK2Node_CallFunction>(Node))
            if (Call->FunctionReference.GetMemberName() == TEXT("TabSwitcherInputs")) InputCall = Call;
    if (!MailCast || !InputCall) return 3;
    auto* Success = MailCast->FindPinChecked(UEdGraphSchema_K2::PN_Then);
    if (Success->LinkedTo.Num() != 1) return 4;
    auto* Refresh = Cast<UK2Node_CallFunction>(Success->LinkedTo[0]->GetOwningNode());
    if (!Refresh || Refresh->FunctionReference.GetMemberName() != TEXT("Refresh Mail Preview")) return 5;
    auto* Continue = Refresh->FindPinChecked(UEdGraphSchema_K2::PN_Then);
    if (Continue->LinkedTo.Num() != 1) return 6;
    auto* Destination = Continue->LinkedTo[0];
    auto* Failed = MailCast->FindPinChecked(TEXT("CastFailed"));
    if (Failed->LinkedTo.Num() && !Failed->LinkedTo.Contains(Destination)) return 7;
    auto* WidgetPin = InputCall->FindPinChecked(TEXT("TabSwitcherWidget"));
    if (!VerifyOnly)
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), TEXT(".uasset"));
        const FString Backup = FPaths::ProjectSavedDir() / TEXT("QA/TabFocusBefore/WB_TabSwitcher.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *File) != COPY_OK) return 8;
        if (!Failed->LinkedTo.Contains(Destination)) AlleyFix::Link(Failed, Destination);
        MailCast->NodeComment = TEXT("VTG tab focus: non-mail pages continue to active-page focus");
        if (WidgetPin->LinkedTo.Num() != 1 || !Cast<UK2Node_Self>(WidgetPin->LinkedTo[0]->GetOwningNode()))
        {
            WidgetPin->BreakAllPinLinks();
            auto* Self = AlleyFix::Add<UK2Node_Self>(KeyGraph, InputCall->NodePosX - 200, InputCall->NodePosY + 150);
            Self->NodeComment = TEXT("VTG tab focus: route keyboard input to this terminal instance");
            Self->AllocateDefaultPins();
            AlleyFix::Link(Self->FindPinChecked(UEdGraphSchema_K2::PN_Self), WidgetPin);
        }
        if (!AlleyFix::Save(BP)) return 9;
    }
    if (!Failed->LinkedTo.Contains(Destination)) return 10;
    if (WidgetPin->LinkedTo.Num() != 1 || !Cast<UK2Node_Self>(WidgetPin->LinkedTo[0]->GetOwningNode())) return 11;
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors) return 12;
    UE_LOG(LogTemp, Display, TEXT("TAB FOCUS PASS: all tab types reach active-page focus; root keyboard input uses its own instance; blueprint compiles."));
    return 0;
}
}
