namespace VTGLevelAudio
{
void SetupSpatialAudio()
{
    auto BackupSpatial=[](UObject* Asset)
    {
        const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FString Copy=FPaths::ProjectDir()/TEXT("Scripts/SpatialActorAudio/Backup")/(Asset->GetName()+TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy),true);
        if(!IFileManager::Get().FileExists(*Copy)) check(IFileManager::Get().Copy(*Copy,*File,false)==COPY_OK);
    };
    auto* Station=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation"));
    auto* Drone=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC"));
    auto* Anim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_FixCar1"));
    auto* Repair=LoadObject<USoundWave>(nullptr,TEXT("/Game/SFX/0InLevel/Station/METLMvmt___Tinkering_Antique_Lock_344_Audio_Antique_Small_Metals"));
    auto* Hum=LoadObject<USoundWave>(nullptr,TEXT("/Game/SFX/IPHA/Electric_Hum_Loop_E"));
    check(Station && Drone && Anim && Repair && Hum);
    auto* Graph=FBlueprintEditorUtils::FindEventGraph(Station); check(Graph);
    UK2Node_CustomEvent* Idle=nullptr;
    for(UEdGraphNode* N:Graph->Nodes) if(auto* E=Cast<UK2Node_CustomEvent>(N)) if(E->CustomFunctionName==TEXT("L3_SetBelloIdle")) Idle=E;
    check(Idle);
    TArray<UK2Node_CallFunction*> DialogueCalls;
    for(UEdGraphNode* N:Graph->Nodes) if(auto* C=Cast<UK2Node_CallFunction>(N))
        if((C->GetName()==TEXT("K2Node_CallFunction_56") || C->GetName()==TEXT("K2Node_CallFunction_7")) && C->FunctionReference.GetMemberName()==TEXT("BeginDialogue")) DialogueCalls.Add(C);
    check(DialogueCalls.Num()==2);
    for(UObject* A:{static_cast<UObject*>(Station),static_cast<UObject*>(Drone),static_cast<UObject*>(Anim)}) BackupSpatial(A);
    auto AttenuationAsset=[&](const TCHAR* Path,float Radius,float Falloff)
    {
        auto* A=LoadObject<USoundAttenuation>(nullptr,Path,nullptr,LOAD_NoWarn);
        if(!A) { A=NewObject<USoundAttenuation>(CreatePackage(Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(A); }
        A->Attenuation.bAttenuate=true; A->Attenuation.bSpatialize=true;
        A->Attenuation.AttenuationShape=EAttenuationShape::Sphere;
        A->Attenuation.AttenuationShapeExtents=FVector(Radius,0,0);
        A->Attenuation.FalloffDistance=Falloff;
        A->Attenuation.DistanceAlgorithm=EAttenuationDistanceModel::Linear;
        A->Attenuation.StereoSpread=0.f;
        Save(A); return A;
    };
    auto* BelloAtt=AttenuationAsset(TEXT("/Game/LevelNSequence/LevelAudio/Attenuation/SA_BelloRepair"),100.f,900.f);
    auto* DroneAtt=AttenuationAsset(TEXT("/Game/SFX/IPHA/SA_DroneHum"),100.f,700.f);
    // Remove only this repair sound's notifies; leave every other animation notify intact.
    const int32 Removed=Anim->Notifies.RemoveAll([&](const FAnimNotifyEvent& E){auto* N=Cast<UAnimNotify_PlaySound>(E.Notify);return N && N->Sound==Repair;});
    Anim->RefreshCacheData(); Save(Anim);
    UE_LOG(LogTemp,Display,TEXT("SPATIAL Removed %d repair notifies"),Removed);
    bool HasSetup=false;
    for(UEdGraphNode* N:Graph->Nodes) if(auto* C=Cast<UK2Node_CallFunction>(N)) if(C->FunctionReference.GetMemberName()==TEXT("SetBelloRepairLoop")) HasSetup=true;
    if(!HasSetup)
    {
        auto* Setup=Call(Graph,UVTGSpatialAudioLibrary::StaticClass(),TEXT("SetBelloRepairLoop"),Idle->NodePosX+250,Idle->NodePosY);
        auto* Bello=Var(Graph,TEXT("Bello"),Idle->NodePosX,Idle->NodePosY+200);
        auto* Helped=Var(Graph,TEXT("L3_Bello_Helped"),Idle->NodePosX,Idle->NodePosY+300);
        auto Old=P(Idle,TEXT("then"))->LinkedTo; P(Idle,TEXT("then"))->BreakAllPinLinks();
        L(Idle,TEXT("then"),Setup,TEXT("execute"));
        for(auto* Pin:Old) check(S()->TryCreateConnection(P(Setup,TEXT("then")),Pin));
        L(Bello,TEXT("Bello"),Setup,TEXT("Bello")); L(Helped,TEXT("L3_Bello_Helped"),Setup,TEXT("Helped"));
        S()->TrySetDefaultObject(*P(Setup,TEXT("RepairSound")),Repair); S()->TrySetDefaultObject(*P(Setup,TEXT("Attenuation")),BelloAtt);
        for(auto* Begin:DialogueCalls)
        {
            auto* Pause=Call(Graph,UVTGSpatialAudioLibrary::StaticClass(),TEXT("SuspendBelloRepairForDialogue"),Begin->NodePosX+350,Begin->NodePosY);
            auto Next=P(Begin,TEXT("then"))->LinkedTo; P(Begin,TEXT("then"))->BreakAllPinLinks();
            L(Begin,TEXT("then"),Pause,TEXT("execute"));
            for(auto* Pin:Next) check(S()->TryCreateConnection(P(Pause,TEXT("then")),Pin));
            L(Bello,TEXT("Bello"),Pause,TEXT("Bello"));
            check(P(Begin,TEXT("self"))->LinkedTo.Num()==1);
            check(S()->TryCreateConnection(P(Begin,TEXT("self"))->LinkedTo[0],P(Pause,TEXT("Narrative"))));
        }
    }
    Compile(Station); Save(Station);
    UVTGSpatialLoopComponent* Component=nullptr;
    for(auto* N:Drone->SimpleConstructionScript->GetAllNodes()) if(N->GetVariableName()==TEXT("ElectricHum")) Component=CastChecked<UVTGSpatialLoopComponent>(N->ComponentTemplate);
    if(!Component)
    {
        auto* N=Drone->SimpleConstructionScript->CreateNode(UVTGSpatialLoopComponent::StaticClass(),TEXT("ElectricHum"));
        Drone->SimpleConstructionScript->AddNode(N); Component=CastChecked<UVTGSpatialLoopComponent>(N->ComponentTemplate);
    }
    Component->Sound=Hum; Component->Attenuation=DroneAtt;
    Component->bLoopEnabled=true; Component->bReactToMovement=true;
    Component->IdleVolume=.08f; Component->MovingVolume=.13f;
    Component->IdlePitch=.95f; Component->MovingPitch=1.3f;
    Component->FullSpeed=300.f; Component->MovementDeadZone=10.f; Component->ResponseSpeed=4.f;
    Compile(Drone); Save(Drone);
}
}

