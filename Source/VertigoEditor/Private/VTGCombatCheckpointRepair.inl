// Reusable checkpoint API + the explicit BlackMarketStreet scene rebuild entries.
struct FVTGEncounterCheckpointRegression
{
 static void Verify(UWorld* W,AVTGLevelManagerBase* Fresh,const FVTGLevelProgressRecord& Record)
 {
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);
  auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;GI->Init();
  auto* Save=GI->GetSubsystem<UVTGSaveCoordinator>();check(Save);
  Save->PendingLoad=NewObject<UVTGSaveGame>();Save->PendingLoad->Meta.LevelName=UGameplayStatics::GetCurrentLevelName(W,true);
  Save->PendingLoad->LevelProgress.Add(Fresh->GetFName(),Record);
  Fresh->DispatchBeginPlay();
  check(Fresh->HasRestoredLevelProgress());
  check(FindFProperty<FBoolProperty>(Fresh->GetClass(),TEXT("L4_MainDlg1Done"))->GetPropertyValue_InContainer(Fresh));
  check(!FindFProperty<FBoolProperty>(Fresh->GetClass(),TEXT("L4_MainDlg3Done"))->GetPropertyValue_InContainer(Fresh));
  check(FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->GetPropertyValue_InContainer(Fresh)==3);
  Save->OnSlotLoaded.Broadcast(0);
  check(FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->GetPropertyValue_InContainer(Fresh)==7);
  check(FindFProperty<FBoolProperty>(Fresh->GetClass(),TEXT("L4_MainDlg3Done"))->GetPropertyValue_InContainer(Fresh));
  FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->SetPropertyValue_InContainer(Fresh,2);
  Save->OnSlotLoaded.Broadcast(0);
  check(FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->GetPropertyValue_InContainer(Fresh)==2);
  auto* Gathered=NewObject<UVTGSaveGame>();Save->GatherWorldState(W,Gathered);
  check(Gathered->LevelProgress.FindChecked(Fresh->GetFName()).EncounterResumeEvent==Record.EncounterResumeEvent);
  Save->PendingLoad=nullptr;
  GI->Shutdown();GEngine->DestroyWorldContext(W);
  UE_LOG(LogTemp,Display,TEXT("ENCOUNTER TIMING PASS: coordinator restores flags before BeginPlay; stage intro skipped; OnSlotLoaded dispatches once; real coordinator gathers resume record"));
 }
};

namespace CombatCheckpoints
{
using namespace MarketCombat;
const FName ResumeFight(TEXT("VTG_ResumeAlleyFight"));
const FName ResumeAfter(TEXT("VTG_ResumeAfterAlleyFight"));
const TCHAR* Mark=TEXT("VTG encounter checkpoint");

UK2Node_CallFunction* Invoke(UEdGraph* G,UBlueprint* BP,FName Name,int X,int Y)
{
 auto* N=Call(G,BP->GeneratedClass->FindFunctionByName(Name),X,Y);N->NodeComment=Mark;return N;
}
void InsertAfter(UEdGraphPin* Out,UEdGraphNode* N)
{
 auto Next=Out->LinkedTo;Out->BreakAllPinLinks();
 check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Out,N->FindPinChecked(TEXT("execute"))));
 for(auto* P:Next)check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(N->FindPinChecked(TEXT("then")),P));
}
void Patch(UBlueprint* BP)
{
 auto* G=EventGraph(BP);check(G);
 if(BP->GeneratedClass->FindFunctionByName(ResumeFight))return;
 // Compile event signatures before generating calls that refer to them.
 auto* Fight=Event(G,ResumeFight,10800,15600);
 auto* After=Event(G,ResumeAfter,10800,16400);
 Fight->NodeComment=After->NodeComment=Mark;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);check(Compile(BP));
 auto Stage=[&](UEdGraphNode* Entry,int Y)
 {
  auto* S=Set(G,BP,TEXT("CurrentStage"),TEXT("None"),11100,Y);
  auto* V=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("GetActiveStage")),10800,Y+180);
  Link(V,TEXT("ReturnValue"),S,TEXT("CurrentStage"));Link(Entry,TEXT("then"),S,TEXT("execute"));return S;
 };
 auto Lock=[&](UEdGraphNode* Tail,bool Enabled,int X,int Y)
 {
  auto* N=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("SetPlayerStoryCombatLock")),X,Y);
  N->FindPinChecked(TEXT("Enabled"))->DefaultValue=Enabled?TEXT("true"):TEXT("false");Link(Tail,TEXT("then"),N,TEXT("execute"));return N;
 };
 UEdGraphNode* Tail=Lock(Stage(Fight,15600),false,11400,15600);
 for(const TCHAR* Name:{TEXT("L4_Alley_BeforeConflictHappens?"),TEXT("L4_FirstTimeTriggerAlley?")})
 {auto* N=Set(G,BP,Name,TEXT("false"),Tail->NodePosX+300,15600);Link(Tail,TEXT("then"),N,TEXT("execute"));Tail=N;}
 int X=12300;
 for(const TCHAR* Name:{TEXT("L4_InitializeAlley"),TEXT("L4_Alley_BallIPHA"),TEXT("L4_GoonInCombat")})
 {auto* N=Invoke(G,BP,Name,X,15600);Link(Tail,TEXT("then"),N,TEXT("execute"));Tail=N;X+=320;}
 // Rebuild arena collision, as in the original alternate entrance; no dialogue or sequences.
 auto* ArenaSource=CastChecked<UK2Node_CallFunction>(Named(G,TEXT("K2Node_CallFunction_189")));
 auto* Arena=Call(G,ArenaSource->GetTargetFunction(),X,15600);
 GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(*Arena->FindPinChecked(TEXT("ActorClass")),ArenaSource->FindPinChecked(TEXT("ActorClass"))->DefaultObject);
 Link(Tail,TEXT("then"),Arena,TEXT("execute"));
 auto* Activate=Call(G,CastChecked<UK2Node_CallFunction>(Named(G,TEXT("K2Node_CallFunction_184")))->GetTargetFunction(),X+320,15600);
 Link(Arena,TEXT("then"),Activate,TEXT("execute"));Link(Arena,TEXT("ReturnValue"),Activate,TEXT("self"));

 Tail=Lock(Stage(After,16400),true,11400,16400);X=11700;
 for(const TCHAR* Name:{TEXT("L4_RemoveEnemy"),TEXT("L4_HideGoonNPC"),TEXT("ReadyToLeaveThe Alley"),TEXT("IPHAFollowsSa")})
 {auto* N=Invoke(G,BP,Name,X,16400);Link(Tail,TEXT("then"),N,TEXT("execute"));Tail=N;X+=320;}
 // The shared DoOnce is the save boundary: both entrances save once, before damage can occur.
 auto* Arm=BP->GeneratedClass->FindFunctionByName(TEXT("InitializeAlleyCombat"));check(Arm);
 UK2Node_CustomEvent* ArmEntry=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_CustomEvent>(N))if(E->CustomFunctionName==TEXT("InitializeAlleyCombat"))ArmEntry=E;
 check(ArmEntry);auto* Gate=ArmEntry->FindPinChecked(TEXT("then"))->LinkedTo[0]->GetOwningNode();
 auto* Save=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("SaveEncounterCheckpoint")),11200,11800);
 Save->NodeComment=Mark;Save->FindPinChecked(TEXT("CheckpointId"))->DefaultValue=TEXT("L4_AlleyFightStart");
 Save->FindPinChecked(TEXT("ResumeEvent"))->DefaultValue=ResumeFight.ToString();
 Save->FindPinChecked(TEXT("Order"))->DefaultValue=TEXT("100");
 auto* Player=Get(G,TEXT("Player Pawn"),10600,11800);
 auto* Transform=Call(G,AActor::StaticClass()->FindFunctionByName(TEXT("GetTransform")),10800,11800);
 Link(Player,TEXT("Player Pawn"),Transform,TEXT("self"));Link(Transform,TEXT("ReturnValue"),Save,TEXT("RespawnTransform"));
 InsertAfter(Gate->FindPinChecked(TEXT("Completed")),Save);
 // Subsequent saves must rebuild the completed scene, rather than resurrecting the encounter.
 auto* Completed=Named(G,TEXT("K2Node_CustomEvent_56"));check(Completed);
 auto* Update=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("SetEncounterResumeEvent")),10400,16800);
 Update->NodeComment=Mark;Update->FindPinChecked(TEXT("ResumeEvent"))->DefaultValue=ResumeAfter.ToString();InsertAfter(Completed->FindPinChecked(TEXT("then")),Update);
 auto* Happened=Set(G,BP,TEXT("L4_FightHappened?"),TEXT("true"),10700,16800);InsertAfter(Update->FindPinChecked(TEXT("then")),Happened);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);check(Compile(BP));
}

void Verify(UBlueprint* BP)
{
 FEditorScriptExecutionGuard ScriptGuard;
 auto* G=EventGraph(BP);int Saves=0;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("SaveEncounterCheckpoint"))
 {
  ++Saves;check(C->FindPinChecked(TEXT("ResumeEvent"))->DefaultValue==ResumeFight.ToString());
  auto* Input=C->FindPinChecked(TEXT("execute"));bool HasOnce=false;
  for(int Step=0;Step<8 && Input && Input->LinkedTo.Num()==1;++Step)
  {
   auto* Upstream=Input->LinkedTo[0]->GetOwningNode();
   if(Cast<UK2Node_MacroInstance>(Upstream)){HasOnce=true;break;}
   Input=Upstream->FindPin(TEXT("execute"));
  }
  check(HasOnce);
 }
 check(Saves==1 && BP->GeneratedClass->FindFunctionByName(ResumeFight) && BP->GeneratedClass->FindFunctionByName(ResumeAfter));
 // Test new/old saves and compound values against a real BPLM class, without touching player slots.
 auto* Copy=DuplicateObject<UBlueprint>(BP,GetTransientPackage(),TEXT("EncounterCheckpointRegression"));check(Copy);
 FEdGraphPinType ArrayType;ArrayType.PinCategory=UEdGraphSchema_K2::PC_Int;ArrayType.ContainerType=EPinContainerType::Array;
 check(FBlueprintEditorUtils::AddMemberVariable(Copy,TEXT("SavedProgressArray"),ArrayType));
 FEdGraphPinType TransformType;TransformType.PinCategory=UEdGraphSchema_K2::PC_Struct;TransformType.PinSubCategoryObject=TBaseStructure<FTransform>::Get();
 check(FBlueprintEditorUtils::AddMemberVariable(Copy,TEXT("SavedProgressTransform"),TransformType));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);check(Compile(Copy));
 auto* CG=EventGraph(Copy);
 // Isolate common references/UI initialization; the native BeginPlay and stage dispatch still run.
 Named(CG,TEXT("K2Node_Event_0"))->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
 auto* StageEntry=Named(CG,TEXT("K2Node_Event_3"));StageEntry->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
 auto* StageRan=Set(CG,Copy,TEXT("L4_MainDlg3Done"),TEXT("true"),0,500);Link(StageEntry,TEXT("then"),StageRan,TEXT("execute"));
 UK2Node_CustomEvent* Resume=nullptr;for(UEdGraphNode* N:CG->Nodes)if(auto* E=Cast<UK2Node_CustomEvent>(N))if(E->CustomFunctionName==ResumeFight)Resume=E;
 check(Resume);Resume->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
 auto* Capture=Set(CG,Copy,TEXT("L4_LeftEnemies"),TEXT("7"),0,0);Link(Resume,TEXT("then"),Capture,TEXT("execute"));
 // Calling save inside resume must short-circuit even without a live coordinator/player.
 auto* NoOverwrite=Call(CG,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("SaveEncounterCheckpoint")),300,0);
 Link(Capture,TEXT("then"),NoOverwrite,TEXT("execute"));
 NoOverwrite->FindPinChecked(TEXT("CheckpointId"))->DefaultValue=TEXT("MustNotOverwrite");
 NoOverwrite->FindPinChecked(TEXT("ResumeEvent"))->DefaultValue=ResumeAfter.ToString();
 auto* Identity=Call(CG,UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("MakeTransform")),300,200);
 Link(Identity,TEXT("ReturnValue"),NoOverwrite,TEXT("RespawnTransform"));
 auto* SaveSkipped=Set(CG,Copy,TEXT("L4_MainDlg3Done"),TEXT("false"),650,0);
 Link(NoOverwrite,TEXT("then"),SaveSkipped,TEXT("execute"));Link(NoOverwrite,TEXT("ReturnValue"),SaveSkipped,TEXT("L4_MainDlg3Done"));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);check(Compile(Copy));
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("EncounterSnapshotTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);check(W);
 auto* First=W->SpawnActor<AVTGLevelManagerBase>(Copy->GeneratedClass);
 auto* Fresh=W->SpawnActor<AVTGLevelManagerBase>(Copy->GeneratedClass);check(First && Fresh);
 Flag(First,TEXT("L4_MainDlg1Done"),true);Flag(First,TEXT("L4_MainDlg2Done"),true);
 auto* Array=FindFProperty<FArrayProperty>(First->GetClass(),TEXT("SavedProgressArray"));check(Array);
 FScriptArrayHelper A(Array,Array->ContainerPtrToValuePtr<void>(First));A.AddValue();CastFieldChecked<FIntProperty>(Array->Inner)->SetPropertyValue(A.GetRawPtr(0),42);
 auto* Transform=FindFProperty<FStructProperty>(First->GetClass(),TEXT("SavedProgressTransform"));check(Transform);
 *Transform->ContainerPtrToValuePtr<FTransform>(First)=FTransform(FRotator(0,70,0),FVector(12,34,56));
 First->ExcludedProgressVariables.Add(TEXT("L4_LeftEnemies"));
 check(First->SetEncounterResumeEvent(ResumeFight));
 auto* Data=NewObject<UVTGSaveGame>();auto& R=Data->LevelProgress.FindOrAdd(TEXT("Manager"));First->CaptureLevelProgress(R);
 check(R.Values.Contains(TEXT("SavedProgressArray")) && R.Values.Contains(TEXT("SavedProgressTransform")));
 check(!R.Values.Contains(TEXT("Player Pawn")) && !R.Values.Contains(TEXT("bBusy")) && !R.Values.Contains(TEXT("L4_LeftEnemies")));
 TArray<uint8> Bytes;check(UGameplayStatics::SaveGameToMemory(Data,Bytes));auto* Loaded=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
 Fresh->RestoreLevelProgress(Loaded->LevelProgress.FindChecked(TEXT("Manager")));
 check(FindFProperty<FBoolProperty>(Fresh->GetClass(),TEXT("L4_MainDlg1Done"))->GetPropertyValue_InContainer(Fresh));
 FScriptArrayHelper B(Array,Array->ContainerPtrToValuePtr<void>(Fresh));check(B.Num()==1 && CastFieldChecked<FIntProperty>(Array->Inner)->GetPropertyValue(B.GetRawPtr(0))==42);
 check(Transform->ContainerPtrToValuePtr<FTransform>(Fresh)->Equals(*Transform->ContainerPtrToValuePtr<FTransform>(First)));
 Fresh->ResumeSavedEncounter();check(FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->GetPropertyValue_InContainer(Fresh)==7);
 FVTGLevelProgressRecord Again;Fresh->CaptureLevelProgress(Again);check(Again.EncounterResumeEvent==ResumeFight);
 FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->SetPropertyValue_InContainer(Fresh,2);
 Fresh->ResumeSavedEncounter();check(FindFProperty<FIntProperty>(Fresh->GetClass(),TEXT("L4_LeftEnemies"))->GetPropertyValue_InContainer(Fresh)==2);
 check(Fresh->SetEncounterResumeEvent(ResumeAfter));Fresh->CaptureLevelProgress(Again);check(Again.EncounterResumeEvent==ResumeAfter);
 auto* Timing=W->SpawnActor<AVTGLevelManagerBase>(Copy->GeneratedClass);
 FVTGEncounterCheckpointRegression::Verify(W,Timing,Loaded->LevelProgress.FindChecked(TEXT("Manager")));
 auto* Old=W->SpawnActor<AVTGLevelManagerBase>(Copy->GeneratedClass);FVTGLevelProgressRecord Legacy;Legacy.ClassPath=Old->GetClass()->GetPathName();Old->RestoreLevelProgress(Legacy);Old->ResumeSavedEncounter();
 check(FindFProperty<FIntProperty>(Old->GetClass(),TEXT("L4_LeftEnemies"))->GetPropertyValue_InContainer(Old)==3);
 Old->DispatchBeginPlay();check(FindFProperty<FBoolProperty>(Old->GetClass(),TEXT("L4_MainDlg3Done"))->GetPropertyValue_InContainer(Old));
 W->DestroyWorld(false);
 UE_LOG(LogTemp,Display,TEXT("ENCOUNTER TEST PASS: real BPLM dialogue flags + array + transform SaveGame roundtrip; runtime refs/locks excluded; resume once, never overwrites checkpoint; old saves retain normal entry; completed resume persisted"));
}

void VerifyMap(UBlueprint* BP)
{
 auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/ThirdPerson/Maps/Street/BlackMarketStreet.BlackMarketStreet"));check(Map && Map->PersistentLevel);
 int Managers=0,Arenas=0;
 for(AActor* Actor:Map->PersistentLevel->Actors)
 {
  if(!Actor)continue;
  if(Actor->GetClass()->GetName()==TEXT("BP_FightingAreaCollision_C"))++Arenas;
  if(!Actor->IsA(BP->GeneratedClass))continue;
  ++Managers;
  for(const TCHAR* Name:{TEXT("Enm_Goon1"),TEXT("Enm_Goon2"),TEXT("Enm_Goon3"),TEXT("NPC_Goon1"),TEXT("NPC_Goon2"),TEXT("NPC_Goon3"),TEXT("IPHADrone"),TEXT("L4_Block_Alley")})
  {
   auto* P=FindFProperty<FObjectPropertyBase>(Actor->GetClass(),Name);check(P && IsValid(P->GetObjectPropertyValue_InContainer(Actor)));
   UE_LOG(LogTemp,Display,TEXT("ENCOUNTER map reference %s = %s"),Name,*GetNameSafe(P->GetObjectPropertyValue_InContainer(Actor)));
  }
 }
 check(Managers==1 && Arenas==1);
 UE_LOG(LogTemp,Display,TEXT("ENCOUNTER MAP PASS: actual BlackMarketStreet manager, three enemies, story NPCs, Drone, barrier and arena references valid"));
}

int32 Run(UBlueprint* BP,UBlueprint* Player,UBlueprint* Collision,bool VerifyOnly)
{
 check(BP->GeneratedClass->IsChildOf(AVTGLevelManagerBase::StaticClass()));
 if(!VerifyOnly)Patch(BP);
 check(Compile(BP));Verify(BP);VerifyMap(BP);MarketCombat::Test(BP,Player,Collision);
 if(!VerifyOnly)
 {
  FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  FString Backup=FPaths::ProjectDir()/TEXT("Scripts/CombatCheckpoints/Backup/BPLM_BlackMarket.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
  if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
 }
 UE_LOG(LogTemp,Display,TEXT("COMBAT_CHECKPOINT_SUCCESS"));return 0;
}
}

