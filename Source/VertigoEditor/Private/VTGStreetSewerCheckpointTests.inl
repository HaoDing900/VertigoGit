#include "Save/VTGSaveCoordinator.h"
#include "VTGGameInstanceBase.h"
#include "EngineUtils.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "LevelSequenceActor.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimSingleNodeInstance.h"

struct FVTGStreetSewerCheckpointRegression
{
 using FRecord=FVTGLevelProgressRecord;
 static UVTGSaveGame* RoundTrip(UVTGSaveCoordinator* Save,UWorld* W,AVTGLevelManagerBase* M,FName Resume)
 {
  check(M->SetEncounterResumeEvent(Resume));auto* Data=NewObject<UVTGSaveGame>();Data->Meta.LevelName=UGameplayStatics::GetCurrentLevelName(W,true);Save->GatherWorldState(W,Data);
  Data->PersistentInts.Add(TEXT("L2StreetFightPhase"),Save->GetPersistentInt(TEXT("L2StreetFightPhase")));
  Data->PersistentInts.Add(TEXT("L2StreetFightAfterDialoguePending"),Save->GetPersistentInt(TEXT("L2StreetFightAfterDialoguePending")));
  const auto& Before=Data->LevelProgress.FindChecked(M->GetFName());check(Before.EncounterResumeEvent==Resume && !Before.Values.Contains(TEXT("Player Pawn")));
  TArray<uint8> Bytes;check(UGameplayStatics::SaveGameToMemory(Data,Bytes));auto* Loaded=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
  const auto& After=Loaded->LevelProgress.FindChecked(M->GetFName());check(Before.Values.OrderIndependentCompareEqual(After.Values) && Before.Types.OrderIndependentCompareEqual(After.Types));
  UE_LOG(LogTemp,Display,TEXT("CHECKPOINT MEMORY PASS %s: %d BPLM value properties round-trip; actor caches excluded"),*M->GetClass()->GetName(),Before.Values.Num());return Loaded;
 }
 static void Restore(UVTGSaveCoordinator* Save,UVTGSaveGame* Data,AVTGLevelManagerBase* M)
 {
  Save->PendingLoad=Data;Save->RestoreLevelManager(M);check(M->HasRestoredLevelProgress());
  const auto& Before=Data->LevelProgress.FindChecked(M->GetFName());FRecord Actual;M->CaptureLevelProgress(Actual);check(Before.Values.OrderIndependentCompareEqual(Actual.Values));
  M->DispatchBeginPlay();
  check(!FindFProperty<FBoolProperty>(M->GetClass(),TEXT("CheckpointRegressionStageRan"))->GetPropertyValue_InContainer(M));
  Save->OnSlotLoaded.Broadcast(0);Save->PendingLoad=nullptr;
 }
 static void Run(UBlueprint* Authored,bool Sewer)
 {
  using namespace MarketFollowup;
  FEditorScriptExecutionGuard Guard;
  auto* BP=DuplicateObject<UBlueprint>(Authored,GetTransientPackage(),Sewer?TEXT("SewerCheckpointRegressionBP"):TEXT("StreetCheckpointRegressionBP"));
  FEdGraphPinType BoolType;BoolType.PinCategory=UEdGraphSchema_K2::PC_Boolean;check(FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("CheckpointRegressionStageRan"),BoolType));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
  auto* G=Graph(BP);
  // Cosmetic inventory/tutorial paths require a rendered HUD; retain scene placement and combat flags.
  if(!Sewer)
  {
   Node(G,TEXT("K2Node_CreateWidget_1"))->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
   Node(G,TEXT("K2Node_CallFunction_126"))->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
  }
  auto* Stage=Node(G,Sewer?TEXT("K2Node_Event_1"):TEXT("K2Node_Event_2"));Stage->FindPinChecked(TEXT("then"))->BreakAllPinLinks();auto* Count=Set(G,TEXT("CheckpointRegressionStageRan"),TEXT("true"),0,0);Link(Stage,TEXT("then"),Count,TEXT("execute"));
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
  auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
  auto* W=UWorld::CreateWorld(EWorldType::Game,false,Sewer?TEXT("SewerCheckpointRegression"):TEXT("StreetCheckpointRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UVTGGameInstanceBase>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;GI->Init();auto* Save=GI->GetSubsystem<UVTGSaveCoordinator>();check(Save);
  FActorSpawnParameters SP;SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* PlayerClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));auto* Player=W->SpawnActor<ACharacter>(PlayerClass,FTransform(FVector(1000,1000,200)),SP);auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);PC->Possess(Player);
  const auto MapPath=Sewer?TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment"):TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar");auto* Map=LoadObject<UWorld>(nullptr,MapPath);check(Map);AVTGLevelManagerBase* Template=nullptr;
  for(AActor* A:Map->PersistentLevel->Actors)if(A && A->GetClass()==Authored->GeneratedClass)Template=CastChecked<AVTGLevelManagerBase>(A);check(Template);
  auto SpawnManager=[&]()
  {
   auto* M=W->SpawnActor<AVTGLevelManagerBase>(BP->GeneratedClass,FTransform::Identity,SP);
   for(TFieldIterator<FObjectPropertyBase> It(Template->GetClass());It;++It)
   {
    auto* Source=Cast<AActor>(It->GetObjectPropertyValue_InContainer(Template));if(!Source || Source==Template || It->GetOwnerClass()==AActor::StaticClass())continue;
    if(auto* P=FindFProperty<FObjectPropertyBase>(M->GetClass(),It->GetFName()))
    {auto TemplateSpawn=SP;TemplateSpawn.Template=Source;auto* Copy=W->SpawnActor<AActor>(Source->GetClass(),Source->GetActorTransform(),TemplateSpawn);P->SetObjectPropertyValue_InContainer(M,Copy);}
   }
   Obj(M,TEXT("Player Pawn"),Player);Obj(M,TEXT("Player Controller"),PC);Obj(M,TEXT("GameInstance"),GI);Obj(M,TEXT("Narrative"),Player->FindComponentByClass<UNarrativeComponent>());return M;
  };
  auto Object=[](UObject* O,FName Name){return FindFProperty<FObjectPropertyBase>(O->GetClass(),Name)->GetObjectPropertyValue_InContainer(O);};
  auto Number=[](UObject* O,FName Name,double Value){auto* P=FindFProperty<FNumericProperty>(O->GetClass(),Name);check(P);if(P->IsFloatingPoint())P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(O),Value);else P->SetIntPropertyValue(P->ContainerPtrToValuePtr<void>(O),int64(Value));};
  auto Run=[](UObject* O,FName Name){O->ProcessEvent(O->FindFunctionChecked(Name),nullptr);};
  auto* Source=SpawnManager();
  if(!Sewer)for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("GetActorOfClass"))
   if(auto* P=C->FindPin(TEXT("ActorClass")))if(auto* Class=Cast<UClass>(P->DefaultObject);Class && !UGameplayStatics::GetActorOfClass(W,Class))W->SpawnActor<AActor>(Class,FTransform::Identity,SP);
  if(Sewer)
  {
   Number(Source,TEXT("L3_MonsterEggKillCount"),4);FindFProperty<FStrProperty>(Source->GetClass(),TEXT("L3_CountedEggIds"))->SetPropertyValue_InContainer(Source,TEXT("|EggBeforeCombat|"));Bool(Source,TEXT("L3_BossEncounterStarted"),true);
  }
  else
  {
   Bool(Source,TEXT("L2_Triggered_Thief"),true);Bool(Source,TEXT("L2_Substage_HadEnteredFight?"),true);Bool(Source,TEXT("L2_EnmSeqPlaying"),false);Save->SetPersistentInt(TEXT("L2StreetFightPhase"),1);
  }
  auto* Data=RoundTrip(Save,W,Source,Sewer?TEXT("VTG_ResumeSewerBossFight"):TEXT("VTG_ResumeL2StreetFight"));const auto Record=Data->LevelProgress.FindChecked(Source->GetFName());
  auto* Fresh=SpawnManager();Data->LevelProgress.Add(Fresh->GetFName(),Record);
  const FTransform Respawn(FRotator(0,75,0),FVector(-400,2100,250));Player->SetActorTransform(Respawn);
  AActor* Boss=nullptr;UActorComponent* Behavior=nullptr;UBlackboardComponent* Blackboard=nullptr;
  if(Sewer)
  {
   Boss=CastChecked<AActor>(Object(Fresh,TEXT("L3_Boss")));for(auto* C:Boss->GetComponents())if(FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))Behavior=C;check(Behavior);
   auto* AI=W->SpawnActor<AAIController>();auto* Tree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/BehaviorTrees/BT_BossAI"));check(AI->UseBlackboard(Tree->BlackboardAsset,Blackboard));Obj(Behavior,TEXT("ControllerRef"),AI);Obj(Behavior,TEXT("CharacterRef"),Boss);
   // Pending restoration blocks AI even if BeginPlay sees a saved Started flag.
   Save->PendingLoad=Data;Save->RestoreLevelManager(Fresh);auto* Encounter=Fresh->FindComponentByClass<UVTGSewerEncounterComponent>();Encounter->BeginPlay();Encounter->TickComponent(.1f,LEVELTICK_All,nullptr);check(!Boss->CanBeDamaged() && !Blackboard->GetValueAsBool(TEXT("InCombat")));
  }
  Restore(Save,Data,Fresh);
  if(Sewer)
  {
   auto* Encounter=Fresh->FindComponentByClass<UVTGSewerEncounterComponent>();Encounter->TickComponent(.1f,LEVELTICK_All,nullptr);check(Boss->CanBeDamaged() && Blackboard->GetValueAsBool(TEXT("InCombat")));check(Encounter->GetMonsterEggKillCount()==4);
   Number(Behavior,TEXT("Health"),0);auto* Death=FindFProperty<FMulticastDelegateProperty>(Behavior->GetClass(),TEXT("OnBossDeath"));Death->GetMulticastDelegate(Death->ContainerPtrToValuePtr<void>(Behavior))->ProcessMulticastDelegate<UObject>(nullptr);
   FRecord Completed;Fresh->CaptureLevelProgress(Completed);check(Completed.EncounterResumeEvent==TEXT("VTG_ResumeAfterSewerBossFight"));
   auto* Post=SpawnManager();auto* AfterData=RoundTrip(Save,W,Fresh,TEXT("VTG_ResumeAfterSewerBossFight"));AfterData->LevelProgress.Add(Post->GetFName(),Completed);Restore(Save,AfterData,Post);
   auto* PostBoss=CastChecked<AActor>(Object(Post,TEXT("L3_Boss")));check(PostBoss->IsHidden() && !PostBoss->GetActorEnableCollision());check(Post->FindComponentByClass<UVTGSewerEncounterComponent>()->GetMonsterEggKillCount()==4);
   Bool(Post,TEXT("L3_CheckpointDoorOpened"),true);Post->FindComponentByClass<UVTGSewerEncounterComponent>()->RestoreEncounterCheckpointScene();auto* Switch=CastChecked<AActor>(Object(Post,TEXT("L3_SwitchColi")));check(FMath::IsNearlyEqual(Switch->GetActorLocation().Z,-136.747789,0.1));
   auto* Door=CastChecked<AActor>(Object(Post,TEXT("RoundDoor")))->FindComponentByClass<USkeletalMeshComponent>();check(Door && Door->GetSingleNodeInstance() && GetNameSafe(Door->GetSingleNodeInstance()->GetCurrentAsset()).Contains(TEXT("open")));
   Bool(Post,TEXT("L3_CheckpointPassedDoor"),true);Post->FindComponentByClass<UVTGSewerEncounterComponent>()->RestoreEncounterCheckpointScene();check(GetNameSafe(Door->GetSingleNodeInstance()->GetCurrentAsset()).Contains(TEXT("Closed")));
   UE_LOG(LogTemp,Display,TEXT("SEWER CHECKPOINT PASS: AI waits for restore; real resume activates fresh boss; egg progress retained; death switches resume; completed load hides boss; switch/door/pass state rebuilt without story replay"));
  }
  else
  {
   auto* Enemy=CastChecked<AActor>(Object(Fresh,TEXT("L2_EnmBarFighter")));check(FVector::Dist(Enemy->GetActorLocation(),FVector(-473.779197,2539.866664,247.590903))<1);
   auto* Arena=CastChecked<AActor>(Object(Fresh,TEXT("FightAreaCollision")));bool Blocking=false;for(auto* C:Arena->GetComponents())if(auto* P=Cast<UPrimitiveComponent>(C))Blocking|=P->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;check(Blocking);
   check(FindFProperty<FBoolProperty>(Fresh->GetClass(),TEXT("L2_Triggered_Thief"))->GetPropertyValue_InContainer(Fresh));check(FindFProperty<FBoolProperty>(Player->GetClass(),TEXT("InCombat"))->GetPropertyValue_InContainer(Player));
   const FVector Moved=Enemy->GetActorLocation()+FVector(25,0,0);Enemy->SetActorLocation(Moved);Save->OnSlotLoaded.Broadcast(0);check(Enemy->GetActorLocation().Equals(Moved));
   auto* State=Fresh->FindComponentByClass<UVTGL2FightStateComponent>();State->CompleteAfterFight(CastChecked<AActor>(Object(Fresh,TEXT("L2_FemaleClient"))));FRecord Completed;Fresh->CaptureLevelProgress(Completed);check(Completed.EncounterResumeEvent==TEXT("VTG_ResumeAfterL2StreetFight"));
   Save->SetPersistentInt(TEXT("L2StreetFightAfterDialoguePending"),0);auto* AfterData=RoundTrip(Save,W,Fresh,TEXT("VTG_ResumeAfterL2StreetFight"));auto* Post=SpawnManager();AfterData->LevelProgress.Add(Post->GetFName(),Completed);Player->SetActorTransform(Respawn);Restore(Save,AfterData,Post);
   auto* Client=CastChecked<AActor>(Object(Post,TEXT("L2_FemaleClient")));auto* AfterState=Post->FindComponentByClass<UVTGL2FightStateComponent>();check(!Client->IsHidden() && Client->GetActorTransform().Equals(AfterState->FemaleClientEndTransform));check(Player->GetActorTransform().Equals(Respawn));check(CastChecked<AActor>(Object(Post,TEXT("L2_EnmBarFighter")))->IsActorBeingDestroyed());
   UE_LOG(LogTemp,Display,TEXT("L2 CHECKPOINT PASS: real resume places fresh enemy; prior story flag retained; combat state enabled; completion switches resume; completed load restores client and removes fight without moving saved player"));
  }
  int Sequences=0;for(TActorIterator<ALevelSequenceActor> It(W);It;++It)++Sequences;check(Sequences==0);
  Save->PendingLoad=nullptr;TArray<AActor*> Actors;for(AActor* A:W->PersistentLevel->Actors)if(A)Actors.Add(A);for(auto* A:Actors){W->GetLatentActionManager().RemoveActionsForObject(A);W->DestroyActor(A);}GI->Shutdown();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
 }
};
namespace StreetSewerCheckpoints
{
void Regression(UBlueprint* L2,UBlueprint* Sewer)
{
 FVTGStreetSewerCheckpointRegression::Run(L2,false);FVTGStreetSewerCheckpointRegression::Run(Sewer,true);
 UE_LOG(LogTemp,Display,TEXT("STREET_SEWER_CHECKPOINT_REGRESSION_SUCCESS: memory serialization and actual OnSlotLoaded scene restoration; no player save slots written"));
}
}
