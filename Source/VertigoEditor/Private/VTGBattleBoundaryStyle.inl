void VTGSetupBoundaryStyle()
{
    using namespace FistWindEditor;
    auto* Parent=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision"));check(Parent);
    const FString Path=TEXT("/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram");
    check(!FPackageName::DoesPackageExist(Path));
    auto* BP=FKismetEditorUtilities::CreateBlueprint(Parent->GeneratedClass,CreatePackage(*Path),TEXT("BP_FightingAreaCollision_BarHologram"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());FAssetRegistryModule::AssetCreated(BP);
    auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/Street/BlackMarketStreet"));check(W);
    UStaticMesh* Mesh=nullptr;UMaterialInterface* Material=nullptr;FQuat MeshRotation;
    for(TActorIterator<AActor> It(W);It;++It)if(It->GetActorLabel()==TEXT("BPA_StageBlockingVolume_l4BlockAlley"))
        for(UActorComponent* C:It->GetComponents())if(auto* M=Cast<UStaticMeshComponent>(C);M && M->GetStaticMesh())
        {Mesh=M->GetStaticMesh();Material=M->GetMaterial(0);MeshRotation=M->GetComponentQuat();break;}
    check(Mesh && Material);
    // Strongly store the exact mesh and material in the new BP before switching worlds.
    auto* Seed=BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),TEXT("HologramSeed"));
    auto* SeedMesh=CastChecked<UStaticMeshComponent>(Seed->ComponentTemplate);SeedMesh->SetStaticMesh(Mesh);SeedMesh->SetMaterial(0,Material);
    BP->SimpleConstructionScript->AddNode(Seed);
    W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar"));check(W);
    AActor* Old=nullptr;AActor* LM=nullptr;
    for(TActorIterator<AActor> It(W);It;++It)
    {
        if(It->GetClass()==Parent->GeneratedClass)Old=*It;
        if(It->GetClass()->GetName()==TEXT("BPLM_L2StreetBarFight_C"))LM=*It;
    }
    check(Old && LM);
    const FString MapFile=FPackageName::LongPackageNameToFilename(W->GetOutermost()->GetName(),FPackageName::GetMapPackageExtension());
    const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/BattleBoundaryStyle/Backup/L2_StreetOutsideBar.umap");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*MapFile,false)==COPY_OK);
    int32 Count=0;
    for(UActorComponent* C:Old->GetComponents())if(auto* Box=Cast<UBoxComponent>(C))
    {
        const FVector Extent=Box->GetUnscaledBoxExtent()*Box->GetRelativeScale3D();const bool AlongX=Extent.X>Extent.Y;
        const float HalfLength=FMath::Max(Extent.X,Extent.Y);if(HalfLength<100)continue;
        const int32 Num=FMath::Max(2,FMath::RoundToInt(HalfLength*2/175.f));
        const FVector Center=Old->GetActorTransform().InverseTransformPosition(Box->GetComponentLocation());
        for(int32 I=0;I<Num;++I)
        {
            USCS_Node* Node=Count==0?Seed:BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),*FString::Printf(TEXT("Hologram_%02d"),Count));
            if(Count>0)BP->SimpleConstructionScript->AddNode(Node);
            auto* M=CastChecked<UStaticMeshComponent>(Node->ComponentTemplate);M->SetStaticMesh(Mesh);M->SetMaterial(0,Material);
            M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCastShadow(false);M->ComponentTags.AddUnique(TEXT("BattleStyle"));
            const float Offset=FMath::Lerp(-HalfLength+80,HalfLength-80,float(I)/(Num-1));
            M->SetRelativeLocation(FVector(Center.X+(AlongX?Offset:0),Center.Y+(AlongX?0:Offset),64.32f));
            M->SetRelativeRotation(FQuat(FVector::UpVector,AlongX?HALF_PI:0)*MeshRotation);
            M->SetVisibility(false);M->SetHiddenInGame(true);++Count;
        }
    }
    auto* G=FBlueprintEditorUtils::FindEventGraph(BP);const auto* Schema=GetDefault<UEdGraphSchema_K2>();
    for(bool Visible:{true,false})
    {
        const FName Name=Visible?TEXT("ActivateCollision"):TEXT("DeactivateCollision");
        auto* Event=NewObject<UK2Node_Event>(G);G->AddNode(Event);Event->CreateNewGuid();Event->EventReference.SetExternalMember(Name,Parent->GeneratedClass);Event->bOverrideFunction=true;Event->AllocateDefaultPins();
        auto* Super=NewObject<UK2Node_CallParentFunction>(G);G->AddNode(Super);Super->CreateNewGuid();Super->SetFromFunction(Parent->GeneratedClass->FindFunctionByName(Name));Super->AllocateDefaultPins();
        auto* Call=NewObject<UK2Node_CallFunction>(G);G->AddNode(Call);Call->CreateNewGuid();Call->SetFromFunction(UVTGCombatBoundaryLibrary::StaticClass()->FindFunctionByName(TEXT("SetBoundaryStyleVisible")));Call->AllocateDefaultPins();
        auto* Self=NewObject<UK2Node_Self>(G);G->AddNode(Self);Self->CreateNewGuid();Self->AllocateDefaultPins();
        check(Schema->TryCreateConnection(Event->FindPinChecked(TEXT("then")),(Visible?Super->FindPinChecked(TEXT("execute")):Call->FindPinChecked(TEXT("execute")))));
        check(Schema->TryCreateConnection((Visible?Super->FindPinChecked(TEXT("then")):Call->FindPinChecked(TEXT("then"))),(Visible?Call->FindPinChecked(TEXT("execute")):Super->FindPinChecked(TEXT("execute")))));
        check(Schema->TryCreateConnection(Self->FindPinChecked(TEXT("self")),Call->FindPinChecked(TEXT("Boundary"))));
        Schema->TrySetDefaultValue(*Call->FindPinChecked(TEXT("Visible")),Visible?TEXT("true"):TEXT("false"));
        Event->NodePosY=Visible?0:400;Super->NodePosX=300;Super->NodePosY=Event->NodePosY;Call->NodePosX=600;Call->NodePosY=Event->NodePosY;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(Log.NumErrors==0);Save(BP);
    FActorSpawnParameters Spawn;Spawn.OverrideLevel=Old->GetLevel();Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* New=W->SpawnActor<AActor>(BP->GeneratedClass,Old->GetActorTransform(),Spawn);check(New);New->SetActorLabel(TEXT("BP_FightingAreaCollision_BarHologram"));
    for(UActorComponent* C:Old->GetComponents())if(auto* Scene=Cast<USceneComponent>(C))
        for(UActorComponent* D:New->GetComponents())if(auto* Dest=Cast<USceneComponent>(D);Dest && Dest->GetFName()==Scene->GetFName())
        {Dest->Modify();Dest->SetRelativeTransform(Scene->GetRelativeTransform());if(auto* Box=Cast<UBoxComponent>(Scene))CastChecked<UBoxComponent>(Dest)->SetBoxExtent(Box->GetUnscaledBoxExtent());}
    auto* Ref=FindFProperty<FObjectPropertyBase>(LM->GetClass(),TEXT("FightAreaCollision"));check(Ref && Ref->GetObjectPropertyValue_InContainer(LM)==Old);
    LM->Modify();Ref->SetObjectPropertyValue_InContainer(LM,New);W->EditorDestroyActor(Old,true);
    {
        TGuardValue<bool> Allow(GAllowActorScriptExecutionInEditor,true);
        auto* Preview=W->SpawnActor<AActor>(BP->GeneratedClass,New->GetActorTransform(),Spawn);check(Preview);
        for(bool Active:{true,false})
        {
            auto* Function=Preview->FindFunction(Active?TEXT("ActivateCollision"):TEXT("DeactivateCollision"));check(Function);Preview->ProcessEvent(Function,nullptr);
            if(!Active){check(!IsValid(Preview) || Preview->IsActorBeingDestroyed());UE_LOG(LogTemp,Display,TEXT("BOUNDARY_STYLE_TEST deactivate destroys boundary as original"));continue;}
            int32 Verified=0;
            for(UActorComponent* C:Preview->GetComponents())if(auto* M=Cast<UStaticMeshComponent>(C);M && M->ComponentHasTag(TEXT("BattleStyle")))
                {UE_LOG(LogTemp,Display,TEXT("STYLE_CHECK %s visible=%d active=%d mesh=%d material=%d func=%s"),*M->GetName(),M->IsVisible(),Active,M->GetStaticMesh()==Mesh,M->GetMaterial(0)==Material,*Function->GetOuter()->GetName());check(M->IsVisible()==Active && M->GetStaticMesh()==Mesh && M->GetMaterial(0)==Material);++Verified;}
            check(Verified==Count);UE_LOG(LogTemp,Display,TEXT("BOUNDARY_STYLE_TEST state=%d count=%d material preserved"),Active,Verified);
        }
    }
    check(UEditorLoadingAndSavingUtils::SaveMap(W,W->GetOutermost()->GetName()));
    UE_LOG(LogTemp,Display,TEXT("BOUNDARY_STYLE_SUCCESS mesh=%s material=%s count=%d"),*Mesh->GetPathName(),*Material->GetPathName(),Count);
}



