namespace VTGUIFeedback
{
void InstallResponseAudio(bool Verify)
{
    const auto* Defaults=GetDefault<UVTGUIFeedbackLibrary>();
    for(bool Hover:{false,true})
    {
        const auto& Pool=Hover?Defaults->ResponseHoverSounds:Defaults->ResponseSounds;
        check(Pool.Num()==3);
        for(int32 I=0;I<3;++I)
        {
            auto* Sound=Pool[I].Get();check(Sound && !Sound->IsLooping());
            check(Sound->GetName()==FString::Printf(TEXT("NSFX_Response%s%d"),Hover?TEXT("Hover"):TEXT(""),I+1));
            UE_LOG(LogTemp,Display,TEXT("RESPONSE AUDIO SOURCE %s duration=%.3f"),*Sound->GetPathName(),Sound->GetDuration());
        }
    }
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/WBP_NarrativeButton_DialogueOption_ProfilePicture"));check(BP);
    if(!Verify)Backup(BP);
    auto* Graph=BP->UbergraphPages[0].Get();
    auto Named=[&](const TCHAR* Name)->UEdGraphNode*
    {
        for(UEdGraphNode* Node:Graph->Nodes)if(Node->GetName()==Name)return Node;
        return nullptr;
    };
    auto FindCall=[&](FName Name)->UK2Node_CallFunction*
    {
        UK2Node_CallFunction* Result=nullptr;
        for(UEdGraphNode* Node:Graph->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(Node))
            if(Call->FunctionReference.GetMemberName()==Name){check(!Result);Result=Call;}
        return Result;
    };
    auto NewCall=[&](FName Name)
    {
        auto* Call=Add<UK2Node_CallFunction>(Graph);
        Call->SetFromFunction(UVTGUIFeedbackLibrary::StaticClass()->FindFunctionByName(Name));Call->AllocateDefaultPins();
        auto* Self=Add<UK2Node_Self>(Graph);Self->AllocateDefaultPins();
        Link(Self->FindPinChecked(TEXT("self")),Call->FindPinChecked(TEXT("Button")));
        Call->NodeComment=TEXT("VTG response feedback: independent audio, never delays dialogue");Call->bCommentBubbleVisible=true;
        return Call;
    };
    auto* Confirm=FindCall(TEXT("ConfirmResponse"));
    if(!Confirm)
    {
        check(!Verify);auto* Old=FindCall(TEXT("TrySelectDialogueOption"));check(Old);
        Confirm=NewCall(TEXT("ConfirmResponse"));Confirm->NodePosX=Old->NodePosX;Confirm->NodePosY=Old->NodePosY;
        for(const auto& Pair:TArray<TPair<FName,FName>>{{TEXT("execute"),TEXT("execute")},{TEXT("then"),TEXT("then")},{TEXT("self"),TEXT("NarrativeComponent")},{TEXT("Option"),TEXT("Option")}})
        {
            auto* Pin=Old->FindPinChecked(Pair.Key);const auto Links=Pin->LinkedTo;Pin->BreakAllPinLinks();
            for(auto* Other:Links)Link(Other,Confirm->FindPinChecked(Pair.Value));
        }
        FBlueprintEditorUtils::RemoveNode(BP,Old,true);
    }
    check(!FindCall(TEXT("TrySelectDialogueOption")));
    auto InsertBefore=[&](FName Function,UEdGraphNode* Target)
    {
        auto* Call=FindCall(Function);
        if(!Call)
        {
            check(!Verify);Call=NewCall(Function);Call->NodePosX=Target->NodePosX-320;Call->NodePosY=Target->NodePosY-160;
            auto* Input=Target->FindPinChecked(TEXT("execute"));const auto Links=Input->LinkedTo;check(!Links.IsEmpty());Input->BreakAllPinLinks();
            for(auto* Other:Links)Link(Other,Call->FindPinChecked(TEXT("execute")));
            Link(Call->FindPinChecked(TEXT("then")),Input);
        }
        check(Call->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Target->FindPinChecked(TEXT("execute"))));return Call;
    };
    // Both real and simulated hover converge here, after unhovering the previous option.
    auto* Hover=InsertBefore(TEXT("HoverResponse"),Named(TEXT("K2Node_CallFunction_16")));
    if(!Verify && Hover->FindPinChecked(TEXT("Dialogue"))->LinkedTo.IsEmpty())
    {
        Link(Named(TEXT("K2Node_VariableGet_4"))->FindPinChecked(TEXT("CurrentDialogue")),Hover->FindPinChecked(TEXT("Dialogue")));
        Link(Named(TEXT("K2Node_VariableGet_13"))->FindPinChecked(TEXT("PlayerReply")),Hover->FindPinChecked(TEXT("Option")));
    }
    auto* Unhover=InsertBefore(TEXT("UnhoverResponse"),Named(TEXT("K2Node_CallFunction_21")));
    UK2Node_Event* Destruct=nullptr;
    for(UEdGraphNode* Node:Graph->Nodes)if(auto* Event=Cast<UK2Node_Event>(Node))if(Event->EventReference.GetMemberName()==TEXT("Destruct"))Destruct=Event;
    if(!Destruct)
    {
        check(!Verify);Destruct=Add<UK2Node_Event>(Graph);Destruct->EventReference.SetExternalMember(TEXT("Destruct"),UUserWidget::StaticClass());Destruct->bOverrideFunction=true;Destruct->AllocateDefaultPins();
    }
    auto* Release=FindCall(TEXT("ReleaseResponseWidget"));
    if(!Release)
    {
        check(!Verify);Release=NewCall(TEXT("ReleaseResponseWidget"));
        // Teardown only releases audio; do not run the visual unhover graph on a dying widget.
        Destruct->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
        Link(Destruct->FindPinChecked(TEXT("then")),Release->FindPinChecked(TEXT("execute")));
    }
    check(Destruct->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Release->FindPinChecked(TEXT("execute"))));
    for(auto* Call:{Confirm,Hover,Unhover})check(!Call->FindPinChecked(TEXT("Button"))->LinkedTo.IsEmpty() && !Call->FindPinChecked(TEXT("execute"))->LinkedTo.IsEmpty());
    check(!Hover->FindPinChecked(TEXT("Dialogue"))->LinkedTo.IsEmpty() && !Hover->FindPinChecked(TEXT("Option"))->LinkedTo.IsEmpty());
    check(!Confirm->FindPinChecked(TEXT("NarrativeComponent"))->LinkedTo.IsEmpty() && !Confirm->FindPinChecked(TEXT("Option"))->LinkedTo.IsEmpty());
    for(auto* Target:{Named(TEXT("K2Node_VariableGet_25")),static_cast<UEdGraphNode*>(Unhover)})
    {
        check(Target);auto* Input=Target->FindPinChecked(TEXT("execute"));
        const FString Marker=TEXT("VTG confirmed appearance lock: ")+Target->GetName();
        UK2Node_IfThenElse* Gate=nullptr;
        for(UEdGraphNode* Node:Graph->Nodes)if(Node->NodeComment==Marker)Gate=CastChecked<UK2Node_IfThenElse>(Node);
        if(!Gate)
        {
            check(!Verify);Gate=Add<UK2Node_IfThenElse>(Graph);Gate->AllocateDefaultPins();Gate->NodeComment=Marker;
            auto* Allowed=NewCall(TEXT("CanUpdateResponseAppearance"));
            Link(Allowed->FindPinChecked(TEXT("ReturnValue")),Gate->FindPinChecked(TEXT("Condition")));
            const auto Old=Input->LinkedTo;Input->BreakAllPinLinks();for(auto* Pin:Old)Link(Pin,Gate->FindPinChecked(TEXT("execute")));
            Link(Gate->FindPinChecked(TEXT("then")),Input);
        }
        check(Input->LinkedTo.Contains(Gate->FindPinChecked(TEXT("then"))));
    }
    Confirm->NodeComment=TEXT("NarrativeSFX: hold confirmed appearance, fade, pause, then submit once");
    Compile(BP);
    const auto* Button=CastChecked<UCommonButtonBase>(BP->GeneratedClass->GetDefaultObject());
    if(const auto* Style=Button->GetStyle())
        UE_LOG(LogTemp,Display,TEXT("RESPONSE AUDIO existing style=%s pressed=%s hovered=%s"),*Style->GetPathName(),*GetPathNameSafe(Style->PressedSlateSound.GetResourceObject()),*GetPathNameSafe(Style->HoveredSlateSound.GetResourceObject()));
    if(!Verify)Save(BP);
    UE_LOG(LogTemp,Display,TEXT("RESPONSE AUDIO PASS: 3 confirmation / 3 hover; shared mouse and simulated paths; compile passed"));
}
}
