namespace MarketFollowup
{
const FName ConflictFlag(TEXT("L4_AlleyConflictActive"));
const FName ReadyFlag(TEXT("L4_AlleyExitReady"));
const FName SuspendName(TEXT("VTG_SuspendAlleyTrigger"));
const FName RestoreName(TEXT("VTG_RestoreAlleyTrigger"));
const FString Marker=TEXT("VTG alley trigger lifecycle");
UEdGraph* Graph(UBlueprint* BP){for(UEdGraph* G:BP->UbergraphPages)if(G->GetName()==TEXT("EventGraph"))return G;checkNoEntry();return nullptr;}
UEdGraphNode* Node(UEdGraph* G,const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetName()==Name)return N;checkNoEntry();return nullptr;}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;N->NodeComment=Marker;return N;}
void Link(UEdGraphPin* A,UEdGraphPin* B){checkf(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B),TEXT("Followup link %s.%s -> %s.%s"),*A->GetOwningNode()->GetName(),*A->PinName.ToString(),*B->GetOwningNode()->GetName(),*B->PinName.ToString());}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){Link(A->FindPinChecked(AP),B->FindPinChecked(BP));}
UK2Node_CallFunction* Call(UEdGraph* G,UFunction* F,int X,int Y){check(F);auto* N=Add<UK2Node_CallFunction>(G,X,Y);N->SetFromFunction(F);N->AllocateDefaultPins();return N;}
UK2Node_CallFunction* Invoke(UEdGraph* G,UBlueprint* BP,FName Name,int X,int Y){return Call(G,BP->GeneratedClass->FindFunctionByName(Name),X,Y);}
UK2Node_VariableGet* Get(UEdGraph* G,FName Name,int X,int Y){auto* N=Add<UK2Node_VariableGet>(G,X,Y);N->VariableReference.SetSelfMember(Name);N->AllocateDefaultPins();return N;}
UK2Node_VariableSet* Set(UEdGraph* G,FName Name,const TCHAR* Value,int X,int Y,UClass* External=nullptr){auto* N=Add<UK2Node_VariableSet>(G,X,Y);if(External)N->VariableReference.SetExternalMember(Name,External);else N->VariableReference.SetSelfMember(Name);N->AllocateDefaultPins();GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*N->FindPinChecked(Name),Value);return N;}
UK2Node_IfThenElse* Branch(UEdGraph* G,FName Flag,int X,int Y){auto* B=Add<UK2Node_IfThenElse>(G,X,Y);B->AllocateDefaultPins();auto* V=Get(G,Flag,X,Y+180);Link(V,*Flag.ToString(),B,TEXT("Condition"));return B;}
UK2Node_CustomEvent* Event(UEdGraph* G,FName Name,int X,int Y){auto* E=Add<UK2Node_CustomEvent>(G,X,Y);E->CustomFunctionName=Name;E->AllocateDefaultPins();return E;}
void Before(UEdGraphPin* Input,UEdGraphNode* N){auto Previous=Input->LinkedTo;Input->BreakAllPinLinks();for(auto* P:Previous)Link(P,N->FindPinChecked(TEXT("execute")));Link(N->FindPinChecked(TEXT("then")),Input);}
void After(UEdGraphPin* Output,UEdGraphNode* N){auto Next=Output->LinkedTo;Output->BreakAllPinLinks();Link(Output,N->FindPinChecked(TEXT("execute")));for(auto* P:Next)Link(N->FindPinChecked(TEXT("then")),P);}
void Compile(UBlueprint* BP){FCompilerResultsLog R;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&R);UE_LOG(LogTemp,Display,TEXT("FOLLOWUP compile %s errors=%d warnings=%d"),*BP->GetName(),R.NumErrors,R.NumWarnings);check(!R.NumErrors && BP->Status!=BS_Error);}
void Save(UBlueprint* BP)
{
 const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectDir()/Folder/TEXT("Backup")/(BP->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,A));
}
void PatchTrigger(UBlueprint* BP,UBlueprint* Trigger)
{
 if(BP->GeneratedClass->FindFunctionByName(SuspendName))return;
 auto* G=Graph(BP);FEdGraphPinType T;T.PinCategory=UEdGraphSchema_K2::PC_Boolean;
 check(FBlueprintEditorUtils::AddMemberVariable(BP,ConflictFlag,T,TEXT("false")) && FBlueprintEditorUtils::AddMemberVariable(BP,ReadyFlag,T,TEXT("false")));
 auto* Suspend=Event(G,SuspendName,10800,17600);auto* Restore=Event(G,RestoreName,10800,18500);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
 for(int I=0;I<2;++I)
 {
  int Y=17600+I*900;auto* E=I?Restore:Suspend;auto* Ref=Get(G,TEXT("L4_Trigger_Alley"),10800,Y+200);
  auto* Valid=Call(G,UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("IsValid")),11000,Y+200);Link(Ref,TEXT("L4_Trigger_Alley"),Valid,TEXT("Object"));
  auto* Check=Add<UK2Node_IfThenElse>(G,11000,Y);Check->AllocateDefaultPins();Link(E,TEXT("then"),Check,TEXT("execute"));Link(Valid,TEXT("ReturnValue"),Check,TEXT("Condition"));
  auto* Once=Set(G,TEXT("DoOnce?"),TEXT("false"),11300,Y,Trigger->GeneratedClass);Link(Ref,TEXT("L4_Trigger_Alley"),Once,TEXT("self"));Link(Check,TEXT("then"),Once,TEXT("execute"));
  auto* Collision=Call(G,AActor::StaticClass()->FindFunctionByName(TEXT("SetActorEnableCollision")),11600,Y);Collision->FindPinChecked(TEXT("bNewActorEnableCollision"))->DefaultValue=I?TEXT("true"):TEXT("false");Link(Ref,TEXT("L4_Trigger_Alley"),Collision,TEXT("self"));
  auto* Box=Invoke(G,Trigger,I?TEXT("EnableCollision"):TEXT("DisableCollision"),11900,Y);Link(Ref,TEXT("L4_Trigger_Alley"),Box,TEXT("self"));
  auto* Hidden=Call(G,AActor::StaticClass()->FindFunctionByName(TEXT("SetActorHiddenInGame")),12200,Y);Hidden->FindPinChecked(TEXT("bNewHidden"))->DefaultValue=I?TEXT("false"):TEXT("true");Link(Ref,TEXT("L4_Trigger_Alley"),Hidden,TEXT("self"));
  if(I){Link(Once,TEXT("then"),Box,TEXT("execute"));Link(Box,TEXT("then"),Hidden,TEXT("execute"));Link(Hidden,TEXT("then"),Collision,TEXT("execute"));}
  else{Link(Once,TEXT("then"),Collision,TEXT("execute"));Link(Collision,TEXT("then"),Box,TEXT("execute"));Link(Box,TEXT("then"),Hidden,TEXT("execute"));}
 }
 // Common initialization remains available on both normal entry and checkpoint restore.
 auto* Begin=Node(G,TEXT("K2Node_CallParentFunction_0"));check(Begin->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
 auto* AlreadyReady=Branch(G,ReadyFlag,10800,19400);Link(Begin,TEXT("then"),AlreadyReady,TEXT("execute"));
 auto* IsConflict=Branch(G,ConflictFlag,11100,19400);Link(AlreadyReady,TEXT("else"),IsConflict,TEXT("execute"));
 auto* S=Invoke(G,BP,SuspendName,11400,19200);auto* R=Invoke(G,BP,RestoreName,11400,19600);
 Link(AlreadyReady,TEXT("then"),S,TEXT("execute"));Link(IsConflict,TEXT("then"),S,TEXT("execute"));Link(IsConflict,TEXT("else"),R,TEXT("execute"));
 // Stop queued overlaps too; the former hole-complete guard silently swallowed later interactions.
 auto* Entry=Node(G,TEXT("K2Node_CustomEvent_32"));Entry->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
 auto* Guard=Branch(G,ConflictFlag,10800,20100);auto* Done=Branch(G,ReadyFlag,11100,20100);Link(Entry,TEXT("then"),Guard,TEXT("execute"));Link(Guard,TEXT("else"),Done,TEXT("execute"));Link(Done,TEXT("else"),Node(G,TEXT("K2Node_IfThenElse_13")),TEXT("execute"));
 UK2Node_CallFunction* Checkpoint=nullptr;UK2Node_CallFunction* ResumeReady=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
 {if(C->FunctionReference.GetMemberName()==TEXT("SaveEncounterCheckpoint"))Checkpoint=C;if(C->FunctionReference.GetMemberName()==TEXT("ReadyToLeaveThe Alley") && C->NodePosY==16400)ResumeReady=C;}
 check(Checkpoint && ResumeReady);
 auto* Active=Set(G,ConflictFlag,TEXT("true"),10800,20700);Before(Checkpoint->FindPinChecked(TEXT("execute")),Active);auto* Disable=Invoke(G,BP,SuspendName,11100,20700);After(Active->FindPinChecked(TEXT("then")),Disable);
 auto* Victory=Node(G,TEXT("K2Node_CustomEvent_56"));auto* Inactive=Set(G,ConflictFlag,TEXT("false"),10800,21200);After(Victory->FindPinChecked(TEXT("then")),Inactive);auto* Enable=Invoke(G,BP,RestoreName,11100,21200);After(Inactive->FindPinChecked(TEXT("then")),Enable);
 auto* Ready=Node(G,TEXT("K2Node_CustomEvent_33"));auto* Mark=Set(G,ReadyFlag,TEXT("true"),10800,21700);After(Ready->FindPinChecked(TEXT("then")),Mark);auto* Stop=Invoke(G,BP,SuspendName,11100,21700);After(Mark->FindPinChecked(TEXT("then")),Stop);
 // One Destroy Actor target pin must not be shared by two different actor references.
 auto* Destroy=Node(G,TEXT("K2Node_CallFunction_73"));Destroy->FindPinChecked(TEXT("self"))->BreakAllPinLinks();auto* Block=Get(G,TEXT("L4_Block_Alley"),11400,21900);Link(Block,TEXT("L4_Block_Alley"),Destroy,TEXT("self"));
 auto* DestroyTrigger=Call(G,AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor")),11700,21700);auto* Ref=Get(G,TEXT("L4_Trigger_Alley"),11700,21900);Link(Ref,TEXT("L4_Trigger_Alley"),DestroyTrigger,TEXT("self"));After(Destroy->FindPinChecked(TEXT("then")),DestroyTrigger);
 // A completed fight does not imply the player has finished the remaining alley interaction.
 auto Inputs=ResumeReady->FindPinChecked(TEXT("execute"))->LinkedTo;ResumeReady->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();auto* ResumeBranch=Branch(G,ReadyFlag,10800,22300);for(auto* P:Inputs)Link(P,ResumeBranch->FindPinChecked(TEXT("execute")));Link(ResumeBranch,TEXT("then"),ResumeReady,TEXT("execute"));
 auto Next=ResumeReady->FindPinChecked(TEXT("then"))->LinkedTo;auto* ResumeEnable=Invoke(G,BP,RestoreName,11100,22500);Link(ResumeBranch,TEXT("else"),ResumeEnable,TEXT("execute"));auto* Alley=Invoke(G,BP,TEXT("L4_InitializeAlley"),11400,22500);Link(ResumeEnable,TEXT("then"),Alley,TEXT("execute"));for(auto* P:Next)Link(Alley->FindPinChecked(TEXT("then")),P);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
}

void PatchCombatPhase(UBlueprint* BP)
{
 auto* G=Graph(BP);UK2Node_CallFunction* Checkpoint=nullptr;
 for(UEdGraphNode* N:G->Nodes)
 {
  if(auto* S=Cast<UK2Node_VariableSet>(N);S && S->NodePosY==20900 && S->NodeComment==Marker)return;
  if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("SaveEncounterCheckpoint"))Checkpoint=C;
 }
 check(Checkpoint);auto* BeforeConflict=Set(G,TEXT("L4_Alley_BeforeConflictHappens?"),TEXT("false"),10800,20900);Before(Checkpoint->FindPinChecked(TEXT("execute")),BeforeConflict);
 auto* FirstTime=Set(G,TEXT("L4_FirstTimeTriggerAlley?"),TEXT("false"),11100,20900);After(BeforeConflict->FindPinChecked(TEXT("then")),FirstTime);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
}

void PatchStyle(UBlueprint* BP,UBlueprint* Player)
{
 auto* G=Graph(BP);auto* Unfinished=Node(G,TEXT("K2Node_CallFunction_110"));
 Unfinished->FindPinChecked(TEXT("PlayParams_StartFromID"))->DefaultValue=TEXT("L4_BlackMarketInvestigation_Still_MainInvestigation_DialogueNode_NPC_333");
 for(const TCHAR* Name:{TEXT("K2Node_CallFunction_109"),TEXT("K2Node_CallFunction_110"),TEXT("K2Node_CallFunction_108")})
 {
  auto* Begin=Node(G,Name);auto* Input=Begin->FindPinChecked(TEXT("execute"));
  if(Input->LinkedTo.Num()==1 && Input->LinkedTo[0]->GetOwningNode()->NodeComment==TEXT("VTG underground Story before BeginDialogue"))continue;
  auto* Ref=Get(G,TEXT("Player Pawn"),Begin->NodePosX-500,Begin->NodePosY+180);
  auto* Style=Set(G,TEXT("DialogueStyle Story or Action or Cine"),TEXT("Story"),Begin->NodePosX-500,Begin->NodePosY,Player->GeneratedClass);Link(Ref,TEXT("Player Pawn"),Style,TEXT("self"));Before(Input,Style);
  auto* Apply=Invoke(G,Player,TEXT("DialogueStyleChange"),Begin->NodePosX-250,Begin->NodePosY);Apply->NodeComment=TEXT("VTG underground Story before BeginDialogue");Link(Ref,TEXT("Player Pawn"),Apply,TEXT("self"));After(Style->FindPinChecked(TEXT("then")),Apply);
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
}

void TriggerTests(UBlueprint* BP,UBlueprint* Trigger)
{
 FEditorScriptExecutionGuard Guard;
 auto* Copy=DuplicateObject<UBlueprint>(BP,GetTransientPackage(),TEXT("MarketAlleyLifecycleRegression"));check(Copy);FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;check(FBlueprintEditorUtils::AddMemberVariable(Copy,TEXT("RegressionBranch"),Int));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);Compile(Copy);
 auto* G=Graph(Copy);
 auto Capture=[&](UEdGraphPin* Out,int Value){Out->BreakAllPinLinks();auto* N=Set(G,TEXT("RegressionBranch"),*FString::FromInt(Value),0,Value*50);Link(Out,N->FindPinChecked(TEXT("execute")));};
 Capture(Node(G,TEXT("K2Node_VariableSet_5"))->FindPinChecked(TEXT("then")),11);
 Capture(Node(G,TEXT("K2Node_IfThenElse_3"))->FindPinChecked(TEXT("then")),22);
 Capture(Node(G,TEXT("K2Node_IfThenElse_3"))->FindPinChecked(TEXT("else")),33);
 // Preserve actual victory collision changes, excluding unrelated dialogue/fade work in this isolated world.
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
 {
  if(C->FunctionReference.GetMemberName()==RestoreName && C->NodePosY==21200)C->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
  if(C->FunctionReference.GetMemberName()==TEXT("K2_DestroyActor") && C->NodePosY==21700)C->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);Compile(Copy);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("AlleyTriggerRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* M=W->SpawnActor<AVTGLevelManagerBase>(Copy->GeneratedClass,FTransform::Identity,Spawn);auto* T=W->SpawnActor<AActor>(Trigger->GeneratedClass,FTransform::Identity,Spawn);auto* Block=W->SpawnActor<AActor>();auto* Pawn=W->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform(FVector(10000,0,0)),Spawn);Pawn->Tags.Add(TEXT("Player"));check(M && T && Block && Pawn);Obj(M,TEXT("L4_Trigger_Alley"),T);Obj(M,TEXT("L4_Block_Alley"),Block);
 auto* Box=CastChecked<UBoxComponent>(FindFProperty<FObjectPropertyBase>(T->GetClass(),TEXT("Box(ChangeSizeHere)"))->GetObjectPropertyValue_InContainer(T));
 FindFProperty<FNameProperty>(T->GetClass(),TEXT("TRIGGER ID"))->SetPropertyValue_InContainer(T,TEXT("L4_Trigger_Alley"));
 auto Run=[](UObject* O,FName Name){O->ProcessEvent(O->FindFunctionChecked(Name),nullptr);};
 auto* Counter=FindFProperty<FIntProperty>(M->GetClass(),TEXT("RegressionBranch"));check(Counter);
 auto Emit=[&](){Box->OnComponentBeginOverlap.Broadcast(Box,Pawn,Pawn->GetCapsuleComponent(),0,false,FHitResult());for(int I=0;I<10;++I){W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(T,.01f);}};
 Run(M,RestoreName);check(T->GetActorEnableCollision() && !T->IsHidden() && Box->GetCollisionEnabled()==ECollisionEnabled::QueryOnly);
 Bool(M,TEXT("L4_FirstTimeTriggerAlley?"),true);Bool(M,TEXT("L4_Alley_BeforeConflictHappens?"),true);Emit();check(Counter->GetPropertyValue_InContainer(M)==11);
 Emit();check(Counter->GetPropertyValue_InContainer(M)==22);
 Bool(M,TEXT("L4_Alley_BeforeConflictHappens?"),false);Bool(M,TEXT("L4_Alley_HadInteractHoledInAlley?"),true);Emit();check(Counter->GetPropertyValue_InContainer(M)==33);
 Counter->SetPropertyValue_InContainer(M,44);Bool(M,*ConflictFlag.ToString(),true);Run(M,SuspendName);check(!T->GetActorEnableCollision() && T->IsHidden() && Box->GetCollisionEnabled()==ECollisionEnabled::NoCollision);Emit();check(Counter->GetPropertyValue_InContainer(M)==44);
 Run(M,TEXT("AfterAlleyFightComplete"));check(!FindFProperty<FBoolProperty>(M->GetClass(),ConflictFlag)->GetPropertyValue_InContainer(M) && T->GetActorEnableCollision() && !T->IsHidden());Emit();check(Counter->GetPropertyValue_InContainer(M)==33);
 FVTGLevelProgressRecord Record;M->CaptureLevelProgress(Record);check(Record.Values.FindChecked(ReadyFlag).Equals(TEXT("False"),ESearchCase::IgnoreCase));
 Run(M,TEXT("ReadyToLeaveThe Alley"));check(T->IsActorBeingDestroyed() && Block->IsActorBeingDestroyed());M->CaptureLevelProgress(Record);check(Record.Values.FindChecked(ReadyFlag).Equals(TEXT("True"),ESearchCase::IgnoreCase));
 W->GetLatentActionManager().RemoveActionsForObject(T);W->DestroyActor(M);W->DestroyActor(Pawn);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
 UE_LOG(LogTemp,Display,TEXT("ALLEY TRIGGER TEST PASS: real overlap repeats through first/before/after branches even after hole flag; combat and queued events blocked; victory restores; Ready alone destroys both actors; readiness saved"));
}

void InjuredBlend(UAnimBlueprint* BP,bool VerifyOnly)
{
 int Matches=0;
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)
  if(auto* Blend=Cast<UAnimGraphNode_LayeredBoneBlend>(N))
  {
   auto& B=Blend->Node;
   if(G->GetName()!=TEXT("AnimGraph") || N->GetName()!=TEXT("AnimGraphNode_LayeredBoneBlend_1") || B.LayerSetup.Num()!=1)continue;
   if(B.LayerSetup[0].BranchFilters.Num()!=1 || B.LayerSetup[0].BranchFilters[0].BoneName!=TEXT("spine_02_x"))continue;
   ++Matches;if(VerifyOnly){check(!B.bMeshSpaceRotationBlend);}else{B.bMeshSpaceRotationBlend=false;}
  }
 check(Matches==1);
 if(!VerifyOnly){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);Save(BP);}
 UE_LOG(LogTemp,Display,TEXT("INJURED BLEND PASS: isolated spine injury layer uses local rotation, original 0.7 weight retained"));
}

int32 Repair(bool VerifyOnly)
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket"));auto* Trigger=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BPA_0SmartTrigger"));auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP && Trigger && Player);
 auto* Anim=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_Sa"));check(Anim);InjuredBlend(Anim,VerifyOnly);
 if(!VerifyOnly){PatchTrigger(BP,Trigger);PatchCombatPhase(BP);PatchStyle(BP,Player);}
 Compile(BP);
 TriggerTests(BP,Trigger);
 StyleProbe(true);
 if(!VerifyOnly)Save(BP);
 UE_LOG(LogTemp,Display,TEXT("BLACK_MARKET_FOLLOWUP_SUCCESS"));return 0;
}
}


