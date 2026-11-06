void VTGApplyCombatSewerPolish(bool Verify)
{
 using namespace CombatSewerPolish;
 auto* Player=Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
 const FString CuePath=TEXT("/Game/SFX/Character/SaCombat/SC_Sa_DodgeCloth");
 auto* Cue=LoadObject<USoundCue>(nullptr,*CuePath,nullptr,LOAD_NoWarn|LOAD_Quiet);
 if(!Verify)
 {
  if(!Cue){auto* Package=CreatePackage(*CuePath);Cue=NewObject<USoundCue>(Package,TEXT("SC_Sa_DodgeCloth"),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Cue);}
  Backup(Cue);Cue->AllNodes.Empty();
  auto* Random=NewObject<USoundNodeRandom>(Cue);Random->ChildNodes.Empty();Random->Weights.Empty();Random->bRandomizeWithoutReplacement=true;Cue->AllNodes.Add(Random);
  for(const FString& Path:{FString(TEXT("/Game/SFX/FootstepAndFoleySounds/Foley/StereoVersions/Wavs/foley_cloth_light_fast_movement_13")),FString(TEXT("/Game/SFX/FootstepAndFoleySounds/Foley/StereoVersions/Wavs/foley_cloth_light_fast_movement_15")),FString(TEXT("/Game/SFX/FootstepAndFoleySounds/Foley/MonoVersions/Wavs/foley_cloth_light_fast_movement_13"))})
  {auto* Wave=NewObject<USoundNodeWavePlayer>(Cue);Wave->SetSoundWave(Load<USoundWave>(Path));Wave->bLooping=false;Cue->AllNodes.Add(Wave);Random->ChildNodes.Add(Wave);Random->Weights.Add(1.f);}
  Cue->FirstNode=Random;Cue->VolumeMultiplier=.55f;Cue->AttenuationSettings=Component<UVTGPlayerAudioComponent>(Player,TEXT("PlayerAudio"))->StepAttenuation;Cue->PostEditChange();Save(Cue);
 }
 check(Cue && Cast<USoundNodeRandom>(Cue->FirstNode) && Cue->FirstNode->ChildNodes.Num()==3);
 auto Dodges=DodgeMontages(Player);check(Dodges.Num()>=2);
 for(auto* M:Dodges)
 {
  if(!Verify)
  {
   Backup(M);M->Notifies.RemoveAll([](const FAnimNotifyEvent& E){return E.NotifyName==TEXT("VTG_DodgeCloth");});
   auto* N=NewObject<UAnimNotify_PlaySound>(M,NAME_None,RF_Transactional);N->Sound=Cue;N->bFollow=true;N->VolumeMultiplier=1.f;
   auto& E=M->Notifies.AddDefaulted_GetRef();E.Notify=N;E.NotifyName=TEXT("VTG_DodgeCloth");E.Link(M,.01f);E.TriggerTimeOffset=GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);E.Guid=FGuid::NewGuid();M->SortNotifies();M->PostEditChange();Save(M);
  }
  int Count=0;for(const auto& E:M->Notifies)if(E.NotifyName==TEXT("VTG_DodgeCloth")){check(CastChecked<UAnimNotify_PlaySound>(E.Notify)->Sound==Cue);++Count;}check(Count==1);
 }
 auto* Anim=Load<UAnimBlueprint>(TEXT("/Game/Characters/Sa/ABP_Sa"));UEdGraph* AG=nullptr;TArray<UEdGraph*> Graphs;Anim->GetAllGraphs(Graphs);for(auto* G:Graphs)if(G->GetFName()==TEXT("AnimGraph"))AG=G;check(AG);
 UAnimGraphNode_ModifyBone* Bone=nullptr;for(UEdGraphNode* N:AG->Nodes)if(N->GetFName()==TEXT("VTG_PunchDown15"))Bone=Cast<UAnimGraphNode_ModifyBone>(N);
 if(!Bone && !Verify)
 {
  Backup(Anim);UAnimGraphNode_ModifyBone* Previous=nullptr;for(UEdGraphNode* N:AG->Nodes)if(N->GetFName()==TEXT("AnimGraphNode_ModifyBone_6"))Previous=Cast<UAnimGraphNode_ModifyBone>(N);check(Previous);
  Bone=NewObject<UAnimGraphNode_ModifyBone>(AG,TEXT("VTG_PunchDown15"));AG->AddNode(Bone,false,false);Bone->CreateNewGuid();Bone->Node.BoneToModify.BoneName=TEXT("spine_02_x");Bone->Node.RotationMode=BMM_Additive;Bone->Node.RotationSpace=BCS_ComponentSpace;Bone->Node.AlphaInputType=EAnimAlphaInputType::Float;Bone->AllocateDefaultPins();Bone->NodePosX=Previous->NodePosX+300;Bone->NodePosY=Previous->NodePosY;
  const auto* S=GetDefault<UEdGraphSchema_K2>();auto* Output=Previous->FindPinChecked(TEXT("Pose"));auto Destinations=Output->LinkedTo;Output->BreakAllPinLinks();check(S->TryCreateConnection(Output,Bone->FindPinChecked(TEXT("ComponentPose"))));for(auto* To:Destinations)check(S->TryCreateConnection(Bone->FindPinChecked(TEXT("Pose")),To));
  for(const auto& Pair:{TPair<FName,FName>(TEXT("PunchDownRotation"),TEXT("Rotation")),TPair<FName,FName>(TEXT("PunchDownAlpha"),TEXT("Alpha"))})
  {
   auto* V=NewObject<UK2Node_VariableGet>(AG);AG->AddNode(V,false,false);V->CreateNewGuid();V->VariableReference.SetSelfMember(Pair.Key);V->AllocateDefaultPins();V->NodePosX=Bone->NodePosX-180;V->NodePosY=Bone->NodePosY+200;
   auto* Pin=Bone->FindPin(Pair.Value);if(!Pin){const int32 Index=Bone->ShowPinForProperties.IndexOfByPredicate([&](const FOptionalPinFromProperty& Info){return Info.PropertyName==Pair.Value;});check(Index!=INDEX_NONE);Bone->SetPinVisibility(true,Index);Bone->ReconstructNode();Pin=Bone->FindPinChecked(Pair.Value);}check(S->TryCreateConnection(V->FindPinChecked(Pair.Key),Pin));
  }
  Bone->NodeComment=TEXT("Extra 15-degree downward actor-space pitch only during actual unarmed punch montages. No camera or actor rotation.");Compile(Anim);Save(Anim);
 }
 check(Bone && Bone->Node.RotationSpace==BCS_ComponentSpace);
 auto* Free=Load<UDialogueBlueprint>(TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Freemove"));
 if(!Verify){Backup(Free);Free->DialogueTemplate->bAlwaysAutoAdvance=true;Compile(Free);Save(Free);}
 check(CastChecked<UDialogueBlueprintGeneratedClass>(Free->GeneratedClass)->GetDialogueTemplate()->bAlwaysAutoAdvance);
 auto* Manager=Load<UBlueprint>(TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"));auto* G=FBlueprintEditorUtils::FindEventGraph(Manager);check(G);
 auto Find=[&](const TCHAR* Name)->UEdGraphNode*{for(UEdGraphNode* N:G->Nodes)if(N->GetFName()==Name)return N;checkf(false,TEXT("Missing %s"),Name);return nullptr;};
 auto* Start=CastChecked<UK2Node_CallFunction>(Find(TEXT("K2Node_CallFunction_3")));check(Start->FunctionReference.GetMemberName()==TEXT("StartBossEncounter"));
 if(!Verify){Backup(Manager);Start->ReconstructNode();Start->FindPinChecked(TEXT("bWaitForIntroDialogue"))->DefaultValue=TEXT("true");}
 check(Start->FindPinChecked(TEXT("bWaitForIntroDialogue"))->DefaultValue==TEXT("true"));
 for(const TCHAR* Name:{TEXT("K2Node_CallFunction_5"),TEXT("K2Node_CallFunction_11")})
 {
  auto* Begin=Find(Name);const FString WaitName=TEXT("VTG_WaitBossIntro_")+FString(Name);UK2Node_CallFunction* Wait=nullptr;for(UEdGraphNode* N:G->Nodes)if(N->GetName()==WaitName)Wait=Cast<UK2Node_CallFunction>(N);
  if(!Wait && !Verify)
  {
   Wait=NewObject<UK2Node_CallFunction>(G,FName(*WaitName));G->AddNode(Wait,false,false);Wait->CreateNewGuid();Wait->SetFromFunction(UVTGSewerEncounterComponent::StaticClass()->FindFunctionByName(TEXT("WaitForBossIntroDialogue")));Wait->AllocateDefaultPins();Wait->NodePosX=Begin->NodePosX+300;Wait->NodePosY=Begin->NodePosY;
   const auto* S=GetDefault<UEdGraphSchema_K2>();auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){check(S->TryCreateConnection(A,B));};
   auto Old=Begin->FindPinChecked(TEXT("then"))->LinkedTo;Begin->FindPinChecked(TEXT("then"))->BreakAllPinLinks();Link(Begin->FindPinChecked(TEXT("then")),Wait->FindPinChecked(TEXT("execute")));for(auto* To:Old)Link(Wait->FindPinChecked(TEXT("then")),To);
   for(auto* From:Start->FindPinChecked(TEXT("self"))->LinkedTo)Link(From,Wait->FindPinChecked(TEXT("self")));
   for(auto* From:Begin->FindPinChecked(TEXT("self"))->LinkedTo)Link(From,Wait->FindPinChecked(TEXT("Narrative")));
   Link(Begin->FindPinChecked(TEXT("ReturnValue")),Wait->FindPinChecked(TEXT("bDialogueStarted")));
  }
  check(Wait && Begin->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Wait->FindPinChecked(TEXT("execute"))));
 }
 if(!Verify){Compile(Manager);Save(Manager);}
 auto* Table=Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"));if(!Verify)Backup(Table);int Abilities=0;
 for(const auto& Row:Table->GetRowMap())
 {
  auto* A=CastFieldChecked<FArrayProperty>(Field(Table->RowStruct,TEXT("Abilities")));FScriptArrayHelper Array(A,A->ContainerPtrToValuePtr<void>(Row.Value));auto* AS=CastFieldChecked<FStructProperty>(A->Inner)->Struct.Get();
  for(int I=0;I<Array.Num();++I)if(Label(AS,Array.GetRawPtr(I),TEXT("AbilityName")).Contains(TEXT("AcidSpit")))
  {
   ++Abilities;auto* Target=CastFieldChecked<FStructProperty>(Field(AS,TEXT("Targeting")));auto* Values=Target->ContainerPtrToValuePtr<void>(Array.GetRawPtr(I));if(!Verify)Number(Target->Struct,Values,TEXT("MaxDistance"),3600);check(Number(Target->Struct,Values,TEXT("MaxDistance"))==3600);
  }
 }
 check(Abilities==1);if(!Verify)Save(Table);
 auto* Projectile=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));
 if(!Verify)
 {
  Backup(Projectile);Component<UVTGAcidFlightComponent>(Projectile,TEXT("AcidFlight"),true)->MaxTravelDistance=3600;
  Component<UNiagaraComponent>(Projectile,TEXT("AcidVisual"),true)->SetRelativeScale3D(FVector(2.4));
  Component<USphereComponent>(Projectile,TEXT("Sphere"),true)->SetSphereRadius(48);
  for(FName Name:{TEXT("StaticMesh"),TEXT("VisibleAcidCore")})Component<USceneComponent>(Projectile,Name,true)->SetRelativeScale3D(FVector(.48));
  Compile(Projectile);Save(Projectile);
 }
 check(Component<UVTGAcidFlightComponent>(Projectile,TEXT("AcidFlight"))->MaxTravelDistance==3600);
 UE_LOG(LogTemp,Display,TEXT("COMBAT_SEWER_ASSETS_PASS verify=%d"),Verify);
}
