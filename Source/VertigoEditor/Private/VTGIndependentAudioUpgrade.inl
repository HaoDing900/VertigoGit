namespace VTGLevelAudio
{
void UpgradeIndependentAudio()
{
    for (bool bPlay : {true, false})
    {
        const FString Path = Root + TEXT("NE/NE_VTG_Audio_") + (bPlay ? TEXT("PlayOneShot") : TEXT("StopOneShot"));
        auto* BP = LoadObject<UBlueprint>(nullptr, *Path); check(BP);
        const FString Original = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
        const FString Copy = FPaths::ProjectDir()/TEXT("Scripts/IndependentNarrativeAudio/TagPriorityBackup")/(BP->GetName()+TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true);
        if (!IFileManager::Get().FileExists(*Copy)) check(IFileManager::Get().Copy(*Copy,*Original,false)==COPY_OK);
        auto AddMember = [&](const TCHAR* Name, const FEdGraphPinType& Type, const TCHAR* Default, const TCHAR* Tooltip)
        {
            if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Name)==INDEX_NONE) Member(BP,Name,Type,Default);
            FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("Tooltip"),Tooltip);
        };
        AddMember(TEXT("PlaybackID"),NameType(),TEXT("None"),TEXT("Tag takes priority over SoundToStop. One named tag per owning Narrative Component; replaying the tag replaces its previous sound. With no tag, Stop falls back to SoundToStop."));
        if (bPlay)
        {
            FEdGraphPinType Bool; Bool.PinCategory=UEdGraphSchema_K2::PC_Boolean;
            AddMember(TEXT("Loop"),Bool,TEXT("false"),TEXT("Loop this playback. Overrides looping in SoundWave/SoundCue without changing the source asset. Cue graphs repeat as a whole; Wave looping is seamless."));
            AddMember(TEXT("StopWhenDialogueFinished"),Bool,TEXT("true"),TEXT("Stop when the entire dialogue that started this sound ends, including cancellation or replacement. Not at the end of each line. Outside a dialogue, use an explicit Stop for loops."));
            AddMember(TEXT("StopFadeOutTime"),Float(),TEXT("0"),TEXT("Fade-out duration when the owning dialogue finishes, in seconds."));
        }
        else
        {
            AddMember(TEXT("SoundToStop"),Obj(USoundBase::StaticClass()),TEXT(""),TEXT("Fallback only when PlaybackID is None. Stops registered playbacks of this source in the same Narrative Component. Ignored when a tag is provided. Both empty does nothing."));
            BP->NewVariables[FBlueprintEditorUtils::FindNewVariableIndex(BP,TEXT("SoundToStop"))].PropertyFlags |= CPF_AdvancedDisplay;
            AddMember(TEXT("FadeOutTime"),Float(),TEXT("0"),TEXT("Fade-out duration in seconds. Zero stops immediately."));
        }
        auto* Graph=FBlueprintEditorUtils::FindEventGraph(BP); check(Graph);
        const auto OldNodes=Graph->Nodes;
        for(auto Node:OldNodes) FBlueprintEditorUtils::RemoveNode(BP,Node.Get(),true);
        Compile(BP);
        auto* Start=Event(Graph,UNarrativeEvent::StaticClass(),TEXT("ExecuteEvent"));
        auto* Invoke=Call(Graph,UVTGNarrativeAudioLibrary::StaticClass(),bPlay?TEXT("PlayIndependentSound"):TEXT("StopIndependentSound"),600,0);
        L(Start,TEXT("then"),Invoke,TEXT("execute"));
        L(Start,TEXT("Pawn"),Invoke,TEXT("Pawn"));
        L(Start,TEXT("NarrativeComponent"),Invoke,TEXT("NarrativeComponent"));
        TArray<FName> Inputs;
        if(bPlay) Inputs={TEXT("NewSound"),TEXT("PlaybackID"),TEXT("Loop"),TEXT("StopWhenDialogueFinished"),TEXT("FadeInTime"),TEXT("VolumeMultiplier"),TEXT("StopFadeOutTime")};
        else Inputs={TEXT("SoundToStop"),TEXT("PlaybackID"),TEXT("FadeOutTime")};
        int32 Y=180;
        for(FName Input:Inputs)
        {
            auto* Value=Var(Graph,*Input.ToString(),250,Y); Y+=120;
            L(Value,*Input.ToString(),Invoke,*Input.ToString());
        }
        Start->NodeComment=TEXT("Tag-first, scoped to the owning Narrative Component. Source fallback only without a tag. External components must be explicitly registered.");
        Start->bCommentBubbleVisible=true;
        Compile(BP); Save(BP);
    }
}
}
