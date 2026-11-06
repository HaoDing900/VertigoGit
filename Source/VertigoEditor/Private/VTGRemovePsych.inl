namespace VTGLevelAudio
{
void RemovePsychChannel()
{
    auto& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();AR.SearchAllAssets(true);
    TArray<FString> Paths={Root+TEXT("BP_VTG_AudioManager"),TEXT("/Game/LevelNSequence/BPLM"),Root+TEXT("BPLM_AudioExample"),Root+TEXT("BP_VTG_AudioManager_Example")};
    const TSet<FName> Removed={TEXT("Audio_PlayPsych"),TEXT("Audio_StopPsych"),TEXT("Audio_PlayOneShot"),TEXT("Audio_StopOneShot")};
    TSet<FName> Candidates;
    for(const FString& P:Paths){TArray<FName> Refs;AR.GetReferencers(*P,Refs);for(FName Ref:Refs)Candidates.Add(Ref);}
    // Abort before edits if production content still calls the retired shared channel.
    for(FName Package:Candidates)
    {
        if(Paths.Contains(Package.ToString()) || Package.ToString()==Root+TEXT("NE/NE_VTG_Audio_PlayPsych") || Package.ToString()==Root+TEXT("NE/NE_VTG_Audio_StopPsych"))continue;
        TArray<FAssetData> Assets;AR.GetAssetsByPackageName(Package,Assets);
        for(auto& D:Assets)
        {
            if(D.AssetClassPath.GetAssetName()==TEXT("World"))continue;
            if(auto* BP=Cast<UBlueprint>(D.GetAsset()))
            {
                TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
                for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
                    checkf(!Removed.Contains(C->FunctionReference.GetMemberName()),TEXT("Legacy audio call requires migration: %s %s"),*BP->GetPathName(),*C->FunctionReference.GetMemberName().ToString());
            }
        }
    }
    for(const TCHAR* Name:{TEXT("NE_VTG_Audio_PlayPsych"),TEXT("NE_VTG_Audio_StopPsych")})
    {TArray<FName> Refs;AR.GetReferencers(*(Root+TEXT("NE/")+Name),Refs);checkf(Refs.IsEmpty(),TEXT("Referenced Psych NE requires migration: %s"),Name);}
    auto BackupHere=[](UObject* A)
    {
        const FString File=FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FString Copy=FPaths::ProjectDir()/TEXT("Scripts/CutsceneAudio/Backup")/(A->GetName()+TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy),true);if(!IFileManager::Get().FileExists(*Copy))check(IFileManager::Get().Copy(*Copy,*File,false)==COPY_OK);
    };
    TArray<UBlueprint*> Blueprints;
    for(const auto& Path:Paths){auto* B=LoadObject<UBlueprint>(nullptr,*Path);check(B);BackupHere(B);Blueprints.Add(B);}
    for(const TCHAR* Name:{TEXT("NE_VTG_Audio_PlayPsych"),TEXT("NE_VTG_Audio_StopPsych")})
    {
        const FString Package=Root+TEXT("NE/")+Name;
        if(!FPackageName::DoesPackageExist(Package))continue;
        auto* Old=LoadObject<UObject>(nullptr,*Package);check(Old);BackupHere(Old);
        TArray<UObject*> Retired={Old};check(ObjectTools::DeleteObjectsUnchecked(Retired)==1);
    }
    // Remove callers first, then their public forwarding functions and the channel component.
    for(auto* B:Blueprints)
    {
        TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);
        for(auto* G:Graphs)if(!Removed.Contains(G->GetFName()))
        {
            const auto Nodes=G->Nodes;
            for(UEdGraphNode* N:Nodes)
            {
                bool Retire=false;
                if(auto* C=Cast<UK2Node_CallFunction>(N))Retire=Removed.Contains(C->FunctionReference.GetMemberName());
                if(auto* V=Cast<UK2Node_VariableGet>(N))Retire=V->VariableReference.GetMemberName()==TEXT("ExamplePsych");
                if(!Retire)continue;
                auto* Exec=N->FindPin(TEXT("execute"));auto* Then=N->FindPin(TEXT("then"));
                TArray<UEdGraphPin*> Before=Exec?Exec->LinkedTo:TArray<UEdGraphPin*>();TArray<UEdGraphPin*> After=Then?Then->LinkedTo:TArray<UEdGraphPin*>();
                N->BreakAllNodeLinks();for(auto* Prev:Before)for(auto* Next:After)check(S()->TryCreateConnection(Prev,Next));
                FBlueprintEditorUtils::RemoveNode(B,N,true);
            }
        }
        const auto Functions=B->FunctionGraphs;
        for(UEdGraph* G:Functions)if(Removed.Contains(G->GetFName()))FBlueprintEditorUtils::RemoveGraph(B,G);
        if(FBlueprintEditorUtils::FindNewVariableIndex(B,TEXT("ExamplePsych"))!=INDEX_NONE)FBlueprintEditorUtils::RemoveMemberVariable(B,TEXT("ExamplePsych"));
    }
    auto* Manager=Blueprints[0];const auto Nodes=Manager->SimpleConstructionScript->GetAllNodes();
    for(auto* N:Nodes)if(N->GetVariableName()==TEXT("AC_Psych"))Manager->SimpleConstructionScript->RemoveNode(N);
    Compile(Manager);Save(Manager);
    Compile(Blueprints[1]);Save(Blueprints[1]);
    Compile(Blueprints[3]);Save(Blueprints[3]);
    Compile(Blueprints[2]);Save(Blueprints[2]);
    UE_LOG(LogTemp,Display,TEXT("CUTSCENE_AUDIO_REMOVED_PSY manager retains ambience/music only; independent OneShot untouched"));
}
}


