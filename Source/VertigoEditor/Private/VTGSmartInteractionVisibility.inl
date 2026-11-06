namespace VTGUIFeedback
{
void InstallSmartVisibility(bool Verify)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/InventorySystemX/InventorySystemX/Components/AC_PlayerInteractionComponent"));check(BP);
    UEdGraph* Graph=nullptr;for(UEdGraph* Candidate:BP->FunctionGraphs)if(Candidate->GetFName()==TEXT("IsVisibleActor"))Graph=Candidate;check(Graph);
    auto Named=[&](const TCHAR* Name)->UEdGraphNode*{for(UEdGraphNode* Node:Graph->Nodes)if(Node->GetName()==Name)return Node;return nullptr;};
    const FString Marker=TEXT("VTG volume interaction visibility: ignore own restrict wall; accept inside-volume no-hit");
    bool Exists=false;for(UEdGraphNode* Node:Graph->Nodes)Exists|=Node->NodeComment==Marker;
    if(!Exists)
    {
        check(!Verify);Backup(BP);
        auto* Entry=Named(TEXT("K2Node_FunctionEntry_0"));check(Entry);
        auto NewCall=[&](FName Function){auto* Call=Add<UK2Node_CallFunction>(Graph);Call->SetFromFunction(UVTGSmartTriggerComponent::StaticClass()->FindFunctionByName(Function));Call->AllocateDefaultPins();Link(Entry->FindPinChecked(TEXT("Actor")),Call->FindPinChecked(TEXT("Actor")));return Call;};
        auto* Managed=NewCall(TEXT("IsManagedInteractionProxy"));auto* Visible=NewCall(TEXT("IsInteractionProxyVisible"));
        Link(Named(TEXT("K2Node_VariableGet_1"))->FindPinChecked(TEXT("Player Pawn")),Visible->FindPinChecked(TEXT("Player")));
        Link(Named(TEXT("K2Node_CallFunction_20"))->FindPinChecked(TEXT("ReturnValue")),Visible->FindPinChecked(TEXT("CameraLocation")));
        auto* Gate=Add<UK2Node_IfThenElse>(Graph);Gate->AllocateDefaultPins();Gate->NodeComment=Marker;
        auto* Exec=Entry->FindPinChecked(TEXT("then"));const auto Old=Exec->LinkedTo;Exec->BreakAllPinLinks();Link(Exec,Gate->FindPinChecked(TEXT("execute")));for(auto* Pin:Old)Link(Gate->FindPinChecked(TEXT("else")),Pin);
        Link(Managed->FindPinChecked(TEXT("ReturnValue")),Gate->FindPinChecked(TEXT("Condition")));
        auto* Result=DuplicateObject<UEdGraphNode>(Named(TEXT("K2Node_FunctionResult_2")),Graph);Graph->AddNode(Result,false,false);Result->CreateNewGuid();for(auto* Pin:Result->Pins)Pin->LinkedTo.Empty();
        Link(Gate->FindPinChecked(TEXT("then")),Result->FindPinChecked(TEXT("execute")));Link(Visible->FindPinChecked(TEXT("ReturnValue")),Result->FindPinChecked(TEXT("Return")));
    }
    Compile(BP);if(!Verify)Save(BP);
    UE_LOG(LogTemp,Display,TEXT("SMART VISIBILITY PASS: ISX proxy branch installed; ordinary interaction path retained"));
}
}
