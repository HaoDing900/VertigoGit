#include "VTGWidgetDumpCommandlet.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "WidgetBlueprint.h"

namespace WidgetDump
{
FString PinSummary(UEdGraphPin* Pin, int32 Depth = 1)
{
    if (!Pin) return TEXT("?");
    if (Pin->LinkedTo.IsEmpty())
        return Pin->DefaultValue.IsEmpty() ? TEXT("-") : FString::Printf(TEXT("'%s'"), *Pin->DefaultValue);
    UEdGraphNode* Source = Pin->LinkedTo[0]->GetOwningNode();
    FString Title = Source->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "));
    if (Depth > 0)
        for (UEdGraphPin* Up : Source->Pins)
            if (Up->Direction == EGPD_Input && Up->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
                !Up->LinkedTo.IsEmpty())
                Title += TEXT(" ") + PinSummary(Up, Depth - 1);
    return FString::Printf(TEXT("<%s>"), *Title);
}

void Walk(UEdGraphNode* Node, int32 Depth, TSet<UEdGraphNode*>& Seen, const FString& Prefix)
{
    if (!Node || Depth > 40) return;
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
            Walk(Linked->GetOwningNode(), Depth + 1,  Seen,
                 Pin->PinName == FName("then") ? Prefix + TEXT("  ")
                                               : FString::Printf(TEXT("%s  [%s] "), *Prefix, *Pin->PinName.ToString()));
    }
}
}

UVTGWidgetDumpCommandlet::UVTGWidgetDumpCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UVTGWidgetDumpCommandlet::Main(const FString& Params)
{
    using namespace WidgetDump;
    FString Path = TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay");
    FParse::Value(*Params, TEXT("Widget="), Path);
    auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Path);
    check(Blueprint && Blueprint->WidgetTree);

    UE_LOG(LogTemp, Display, TEXT("\n===== widget tree of %s ====="), *Path);
    TFunction<void(UWidget*, const FString&)> Tree = [&](UWidget* Widget, const FString& Prefix)
    {
        if (!Widget) return;
        UE_LOG(LogTemp, Display, TEXT("%s%s : %s  visibility=%d variable=%d"), *Prefix, *Widget->GetName(),
               *Widget->GetClass()->GetName(), (int32)Widget->GetVisibility(), Widget->bIsVariable);
        if (auto* Panel = Cast<UPanelWidget>(Widget))
            for (int32 I = 0; I < Panel->GetChildrenCount(); ++I) Tree(Panel->GetChildAt(I), Prefix + TEXT("  "));
    };
    Tree(Blueprint->WidgetTree->RootWidget, TEXT("  "));

    UE_LOG(LogTemp, Display, TEXT("\n===== graphs ====="));
    for (UEdGraph* Page : Blueprint->UbergraphPages)
    {
        UE_LOG(LogTemp, Display, TEXT("  --- %s (%d nodes) ---"), *Page->GetName(), Page->Nodes.Num());
        TSet<UEdGraphNode*> Seen;
        for (UEdGraphNode* Node : Page->Nodes)
            if (auto* Event = Cast<UK2Node_Event>(Node)) Walk(Event, 0, Seen, TEXT("    "));
    }
    for (UEdGraph* Fn : Blueprint->FunctionGraphs)
    {
        UE_LOG(LogTemp, Display, TEXT("  --- function %s (%d nodes) ---"), *Fn->GetName(), Fn->Nodes.Num());
        TSet<UEdGraphNode*> Seen;
        for (UEdGraphNode* Node : Fn->Nodes)
            if (Node->Pins.ContainsByPredicate([](UEdGraphPin* P)
                    { return P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec; }) &&
                !Node->Pins.ContainsByPredicate([](UEdGraphPin* P)
                    { return P->Direction == EGPD_Input && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec; }))
                Walk(Node, 0, Seen, TEXT("    "));
    }
    UE_LOG(LogTemp, Display, TEXT("== property bindings =="));
    for (const FDelegateEditorBinding& B : Blueprint->Bindings)
        UE_LOG(LogTemp, Display, TEXT("  object=%s property=%s function=%s sourceProperty=%s kind=%d guid=%s pathSegments=%d"),
               *B.ObjectName, *B.PropertyName.ToString(), *B.FunctionName.ToString(), *B.SourceProperty.ToString(),
               (int32)B.Kind, *B.MemberGuid.ToString(), B.SourcePath.Segments.Num());
    return 0;
}

// ---- bindings ------------------------------------------------------------------------------
// Printed by -Bindings so a new property binding can be written in exactly the shape the UMG
// editor already produced for this asset, rather than guessed at.
