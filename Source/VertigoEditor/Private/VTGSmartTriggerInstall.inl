namespace VTGUIFeedback
{
void InstallSmartTriggers(bool Verify)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BPA_0SmartTrigger"));
    auto* Restrict=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BPA_SmartRestrict"));check(BP && Restrict);
    auto Variable=[&](UBlueprint* Asset,FName Name,bool Text,const FString& Default)
    {
        if(FBlueprintEditorUtils::FindNewVariableIndex(Asset,Name)==INDEX_NONE)
        {
            check(!Verify);FEdGraphPinType Type;Type.PinCategory=Text?UEdGraphSchema_K2::PC_Text:UEdGraphSchema_K2::PC_Boolean;
            check(FBlueprintEditorUtils::AddMemberVariable(Asset,Name,Type,Default));
            FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Asset,Name,false);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Asset,Name,nullptr,FText::FromString(TEXT("Smart Trigger")));
        }
    };
    if(!Verify){Backup(BP);Backup(Restrict);}
    Variable(BP,TEXT("RequireExitBeforeRetrigger"),false,TEXT("true"));
    Variable(Restrict,TEXT("RequireInteraction"),false,TEXT("false"));
    Variable(Restrict,TEXT("InteractionPrompt"),true,TEXT("EXAMINE"));
    auto* Graph=BP->UbergraphPages[0].Get();
    auto Named=[&](const TCHAR* Name)->UEdGraphNode*{for(UEdGraphNode* Node:Graph->Nodes)if(Node->GetName()==Name)return Node;return nullptr;};
    auto Call=[&](FName Function,const TCHAR* Marker)->UK2Node_CallFunction*
    {
        for(UEdGraphNode* Node:Graph->Nodes)if(Node->NodeComment==Marker)return CastChecked<UK2Node_CallFunction>(Node);
        check(!Verify);auto* Node=Add<UK2Node_CallFunction>(Graph);Node->SetFromFunction(UVTGSmartTriggerComponent::StaticClass()->FindFunctionByName(Function));Node->AllocateDefaultPins();Node->NodeComment=Marker;
        auto* Self=Add<UK2Node_Self>(Graph);Self->AllocateDefaultPins();Link(Self->FindPinChecked(TEXT("self")),Node->FindPinChecked(TEXT("Trigger")));return Node;
    };
    auto* Init=Call(TEXT("InitializeTrigger"),TEXT("VTG initialize trigger mode"));
    auto* Begin=Named(TEXT("K2Node_Event_0"))->FindPinChecked(TEXT("then"));
    if(!Begin->LinkedTo.Contains(Init->FindPinChecked(TEXT("execute"))))
    {check(!Verify);const auto Old=Begin->LinkedTo;Begin->BreakAllPinLinks();Link(Begin,Init->FindPinChecked(TEXT("execute")));for(auto* Pin:Old)Link(Init->FindPinChecked(TEXT("then")),Pin);}
    auto* Accept=Call(TEXT("AcceptTrigger"),TEXT("VTG accept once per physical visit"));
    auto* Auto=Named(TEXT("K2Node_IfThenElse_4"))->FindPinChecked(TEXT("then"));
    auto* Original=Named(TEXT("K2Node_IfThenElse_1"))->FindPinChecked(TEXT("execute"));
    if(!Auto->LinkedTo.Contains(Accept->FindPinChecked(TEXT("execute"))))
    {
        check(!Verify);Auto->BreakAllPinLinks();Link(Auto,Accept->FindPinChecked(TEXT("execute")));
        Link(Named(TEXT("K2Node_ComponentBoundEvent_0"))->FindPinChecked(TEXT("OtherActor")),Accept->FindPinChecked(TEXT("Player")));
        auto* Gate=Add<UK2Node_IfThenElse>(Graph);Gate->AllocateDefaultPins();Link(Accept->FindPinChecked(TEXT("then")),Gate->FindPinChecked(TEXT("execute")));Link(Accept->FindPinChecked(TEXT("ReturnValue")),Gate->FindPinChecked(TEXT("Condition")));Link(Gate->FindPinChecked(TEXT("then")),Original);
    }
    UK2Node_CustomEvent* Event=nullptr;for(UEdGraphNode* Node:Graph->Nodes)if(auto* E=Cast<UK2Node_CustomEvent>(Node))if(E->CustomFunctionName==TEXT("VTGExecuteAcceptedTrigger"))Event=E;
    if(!Event){check(!Verify);Event=Add<UK2Node_CustomEvent>(Graph);Event->CustomFunctionName=TEXT("VTGExecuteAcceptedTrigger");Event->AllocateDefaultPins();Link(Event->FindPinChecked(TEXT("then")),Original);}
    auto* Finish=Call(TEXT("FinishTriggerDispatch"),TEXT("VTG finish synchronous BPLM dispatch"));
    auto* Completed=Named(TEXT("K2Node_MacroInstance_2"))->FindPinChecked(TEXT("Completed"));
    if(!Completed->LinkedTo.Contains(Finish->FindPinChecked(TEXT("execute")))){check(!Verify);Link(Completed,Finish->FindPinChecked(TEXT("execute")));}
    Compile(BP);Compile(Restrict);
    check(FindFProperty<FBoolProperty>(BP->GeneratedClass,TEXT("RequireExitBeforeRetrigger"))->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
    check(!FindFProperty<FBoolProperty>(Restrict->GeneratedClass,TEXT("RequireInteraction"))->GetPropertyValue_InContainer(Restrict->GeneratedClass->GetDefaultObject()));
    if(!Verify){Save(BP);Save(Restrict);}
    UE_LOG(LogTemp,Display,TEXT("SMART TRIGGER PASS: default automatic; exit required; existing DoOnce/BPLM/camera paths retained"));
}
}
