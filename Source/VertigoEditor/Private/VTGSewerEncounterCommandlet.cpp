#include "VTGSewerEncounterCommandlet.h"
#include "Combat/VTGSewerEncounterComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_SwitchName.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "VTGLevelManagerBase.h"
#include "Kismet/GameplayStatics.h"

namespace SewerSetup
{
const FString ManagerPath=TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt");
const FString BossPath=TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer");
const FString EggPath=TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg");
const FString MapPath=TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment");
template<class T>T* Load(const FString& Path){auto* O=LoadObject<T>(nullptr,*Path);check(O);return O;}
void Backup(UObject* O)
{
 const auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),O->IsA<UWorld>()?FPackageName::GetMapPackageExtension():FPackageName::GetAssetPackageExtension());
 const auto To=FPaths::ProjectDir()/TEXT("Scripts/SewerEncounter/Backup")/FPaths::GetCleanFilename(File);IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true);
 if(!IFileManager::Get().FileExists(*To))check(IFileManager::Get().Copy(*To,*File)==COPY_OK);
}
void Save(UObject* O){auto* P=O->GetOutermost();P->FullyLoad();ResetLoaders(P);P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),O->IsA<UWorld>()?FPackageName::GetMapPackageExtension():FPackageName::GetAssetPackageExtension()),A));}
void Compile(UBlueprint* B){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);}
template<class T>T* Component(UBlueprint* B,FName Name,bool Write)
{
 for(UClass* C=B->GeneratedClass;C;C=C->GetSuperClass())if(auto* P=Cast<UBlueprint>(C->ClassGeneratedBy);P && P->SimpleConstructionScript)
 for(auto* N:P->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==Name)
 {if(P==B || !Write)return CastChecked<T>(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(B->GeneratedClass)));auto* H=B->GetInheritableComponentHandler(true);FComponentKey K(N);auto* R=H->GetOverridenComponentTemplate(K);return CastChecked<T>(R?R:H->CreateOverridenComponentTemplate(K));}
 check(Write);auto* N=B->SimpleConstructionScript->CreateNode(T::StaticClass(),Name);B->SimpleConstructionScript->AddNode(N);return CastChecked<T>(N->ComponentTemplate);
}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;N->NodeComment=TEXT("L3 encounter gate and player egg kill count");return N;}
void Link(UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
void Variable(UBlueprint* B,FName Name,const FName Type,const FString& Description,bool Private=false)
{
 if(FBlueprintEditorUtils::FindNewVariableIndex(B,Name)!=INDEX_NONE)return;
 FEdGraphPinType P;P.PinCategory=Type;check(FBlueprintEditorUtils::AddMemberVariable(B,Name,P));
 const int32 I=FBlueprintEditorUtils::FindNewVariableIndex(B,Name);auto& V=B->NewVariables[I];V.Category=FText::FromString(TEXT("L3 Sewer|Progress"));V.PropertyFlags|=CPF_BlueprintReadOnly|CPF_DisableEditOnInstance;
 FBlueprintEditorUtils::SetBlueprintVariableMetaData(B,Name,nullptr,TEXT("ToolTip"),Description);
 if(Private)FBlueprintEditorUtils::SetBlueprintVariableMetaData(B,Name,nullptr,TEXT("BlueprintPrivate"),TEXT("true"));
}
void Apply()
{
 auto* Manager=Load<UBlueprint>(ManagerPath);auto* Egg=Load<UBlueprint>(EggPath);auto* Boss=Load<UBlueprint>(BossPath);
 Backup(Manager);Backup(Egg);Backup(Boss);
 Variable(Manager,TEXT("L3_MonsterEggKillCount"),UEdGraphSchema_K2::PC_Int,TEXT("Number of Monster Eggs killed by the player. Boss cleanup and duplicate death notifications do not count."));
 Variable(Manager,TEXT("L3_BossEncounterStarted"),UEdGraphSchema_K2::PC_Boolean,TEXT("Set when L3BossEncounter starts; restored with BPLM progress."));
 Variable(Manager,TEXT("L3_BossDefeated"),UEdGraphSchema_K2::PC_Boolean,TEXT("Boss death has already dispatched L3_AfterBossIsDefeated."));
 Variable(Manager,TEXT("L3_CountedEggIds"),UEdGraphSchema_K2::PC_String,TEXT("Internal saved kill IDs, used to prevent duplicate counts after loading."),true);
 Component<UVTGSewerEncounterComponent>(Manager,TEXT("SewerEncounter"),true);
 Component<UVTGEggKillReporterComponent>(Egg,TEXT("PlayerEggKillReporter"),true);
 Compile(Manager);Compile(Egg);
 auto* Behavior=Component<UActorComponent>(Boss,TEXT("Boss Behavior"),true);auto* Trigger=FindFProperty<FByteProperty>(Behavior->GetClass(),TEXT("CombatTrigger"));check(Trigger && Trigger->Enum && Trigger->Enum->GetDisplayNameTextByValue(2).ToString()==TEXT("Take Damage"));Trigger->SetPropertyValue_InContainer(Behavior,2);
 CastChecked<AActor>(Boss->GeneratedClass->GetDefaultObject())->SetCanBeDamaged(false);Compile(Boss);
 UEdGraph* G=Manager->UbergraphPages[0];UK2Node_CustomEvent* EncounterEvent=nullptr;UK2Node_SwitchName* Switch=nullptr;UK2Node_CallFunction* EncounterCall=nullptr;
 for(UEdGraphNode* N:G->Nodes)
 {
  if(auto* E=Cast<UK2Node_CustomEvent>(N);E && E->CustomFunctionName==TEXT("L3BossEncounter"))EncounterEvent=E;
  if(auto* S=Cast<UK2Node_SwitchName>(N);S && S->PinNames.Contains(TEXT("L3_FirstTimeSeeEgg")))Switch=S;
  if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("L3BossEncounter"))EncounterCall=C;
 }
 check(EncounterEvent && Switch && EncounterCall);
 if(!Switch->PinNames.Contains(TEXT("L3BossEncounter"))){Switch->PinNames.Add(TEXT("L3BossEncounter"));Switch->ReconstructNode();}
 EncounterCall->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();Link(Switch->FindPinChecked(TEXT("L3BossEncounter")),EncounterCall->FindPinChecked(TEXT("execute")));
 if(EncounterEvent->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty())
 {
  auto* Get=Add<UK2Node_VariableGet>(G,EncounterEvent->NodePosX+220,EncounterEvent->NodePosY+150);Get->VariableReference.SetSelfMember(TEXT("SewerEncounter"));Get->AllocateDefaultPins();
  auto* Start=Add<UK2Node_CallFunction>(G,EncounterEvent->NodePosX+450,EncounterEvent->NodePosY);Start->SetFromFunction(UVTGSewerEncounterComponent::StaticClass()->FindFunctionByName(TEXT("StartBossEncounter")));Start->AllocateDefaultPins();
  Link(EncounterEvent->FindPinChecked(TEXT("then")),Start->FindPinChecked(TEXT("execute")));Link(Get->FindPinChecked(TEXT("SewerEncounter")),Start->FindPinChecked(TEXT("self")));
 }
 Compile(Manager);Save(Manager);Save(Egg);Save(Boss);
 auto* World=Load<UWorld>(MapPath);Backup(World);AActor* LM=nullptr;AActor* BossActor=nullptr;AActor* SmartTrigger=nullptr;
 for(AActor* Actor:World->PersistentLevel->Actors)
 {
  if(!Actor)continue;
  if(Actor->IsA(Manager->GeneratedClass))LM=Actor;
  if(Actor->IsA(Boss->GeneratedClass))BossActor=Actor;
  if(auto* P=FindFProperty<FNameProperty>(Actor->GetClass(),TEXT("TRIGGER ID"));P && P->GetPropertyValue_InContainer(Actor)==TEXT("L3BossEncounter")){check(!SmartTrigger);SmartTrigger=Actor;}
 }
 UE_LOG(LogTemp,Display,TEXT("Encounter references: Manager=%s Boss=%s Trigger=%s"),*GetNameSafe(LM),*GetNameSafe(BossActor),*GetNameSafe(SmartTrigger));
 check(LM && BossActor && SmartTrigger);
 auto* BossRef=FindFProperty<FObjectPropertyBase>(LM->GetClass(),TEXT("L3_Boss"));auto* TriggerRef=FindFProperty<FObjectPropertyBase>(LM->GetClass(),TEXT("L3_BossTrigger"));check(BossRef && TriggerRef);
 check(!BossRef->GetObjectPropertyValue_InContainer(LM) || BossRef->GetObjectPropertyValue_InContainer(LM)==BossActor);BossRef->SetObjectPropertyValue_InContainer(LM,BossActor);TriggerRef->SetObjectPropertyValue_InContainer(LM,SmartTrigger);
 BossActor->SetCanBeDamaged(false);
 for(auto* C:BossActor->GetComponents())if(auto* P=FindFProperty<FByteProperty>(C->GetClass(),TEXT("CombatTrigger")))P->SetPropertyValue_InContainer(C,2);
 Save(World);UE_LOG(LogTemp,Display,TEXT("SEWER ENCOUNTER APPLY PASS: references, explicit Trigger ID, start gate, death event and egg counter installed"));
}
void Verify()
{
 TGuardValue<bool> ScriptExecution(GAllowActorScriptExecutionInEditor,true);
 auto* Manager=Load<UBlueprint>(ManagerPath);auto* Egg=Load<UBlueprint>(EggPath);auto* Boss=Load<UBlueprint>(BossPath);
 check(Component<UVTGEggKillReporterComponent>(Egg,TEXT("PlayerEggKillReporter"),false));
 auto* Template=Component<UActorComponent>(Boss,TEXT("Boss Behavior"),false);
 check(FindFProperty<FByteProperty>(Template->GetClass(),TEXT("CombatTrigger"))->GetPropertyValue_InContainer(Template)==2);
 check(!CastChecked<AActor>(Boss->GeneratedClass->GetDefaultObject())->CanBeDamaged());
 UEdGraph* G=Manager->UbergraphPages[0];bool Routed=false,Started=false;
 for(UEdGraphNode* N:G->Nodes)
 {
  if(auto* S=Cast<UK2Node_SwitchName>(N);S && S->PinNames.Contains(TEXT("L3_FirstTimeSeeEgg")))
  {auto* P=S->FindPinChecked(TEXT("L3BossEncounter"));check(P->LinkedTo.Num()==1);auto* Call=CastChecked<UK2Node_CallFunction>(P->LinkedTo[0]->GetOwningNode());check(Call->FunctionReference.GetMemberName()==TEXT("L3BossEncounter"));check(S->FindPinChecked(TEXT("Default"))->LinkedTo.IsEmpty());Routed=true;}
  if(auto* E=Cast<UK2Node_CustomEvent>(N);E && E->CustomFunctionName==TEXT("L3BossEncounter"))
  {check(E->FindPinChecked(TEXT("then"))->LinkedTo.Num()==1);auto* Call=CastChecked<UK2Node_CallFunction>(E->FindPinChecked(TEXT("then"))->LinkedTo[0]->GetOwningNode());check(Call->FunctionReference.GetMemberName()==TEXT("StartBossEncounter"));Started=true;}
 }
 check(Routed && Started && Manager->GeneratedClass->FindFunctionByName(TEXT("L3_AfterBossIsDefeated")));
 auto* Map=Load<UWorld>(MapPath);bool Hooked=false;
 for(AActor* A:Map->PersistentLevel->Actors)if(A && A->IsA(Manager->GeneratedClass))
 {
  auto* BossRef=FindFProperty<FObjectPropertyBase>(A->GetClass(),TEXT("L3_Boss"));auto* TriggerRef=FindFProperty<FObjectPropertyBase>(A->GetClass(),TEXT("L3_BossTrigger"));
  auto* B=CastChecked<AActor>(BossRef->GetObjectPropertyValue_InContainer(A));auto* T=CastChecked<AActor>(TriggerRef->GetObjectPropertyValue_InContainer(A));
  check(B->IsA(Boss->GeneratedClass) && !B->CanBeDamaged());check(FindFProperty<FNameProperty>(T->GetClass(),TEXT("TRIGGER ID"))->GetPropertyValue_InContainer(T)==TEXT("L3BossEncounter"));Hooked=true;
 }
 check(Hooked);
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SewerEncounterTests"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* LM=CastChecked<AVTGLevelManagerBase>(World->SpawnActor<AActor>(Manager->GeneratedClass,FTransform::Identity,Spawn));
 auto* Encounter=LM->FindComponentByClass<UVTGSewerEncounterComponent>();check(Encounter && Encounter->GetMonsterEggKillCount()==0);
 auto* Player=World->SpawnActor<APawn>(APawn::StaticClass(),FTransform::Identity,Spawn);auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Player);
 check(UGameplayStatics::GetPlayerPawn(World,0)==Player);
 bool ManagerFound=false;for(TActorIterator<AActor> It(World);It;++It)if(*It==LM)ManagerFound=true;check(ManagerFound);
 auto* Other=World->SpawnActor<AActor>();auto* Weapon=World->SpawnActor<AActor>();Weapon->SetOwner(Player);
 auto SetHealth=[](AActor* A,double H){for(auto* C:A->GetComponents())if(auto* P=FindFProperty<FNumericProperty>(C->GetClass(),TEXT("Health"))){P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(C),H);return;}check(false);};
 auto MakeEgg=[&](){auto* A=World->SpawnActor<AActor>(Egg->GeneratedClass,FTransform::Identity,Spawn);SetHealth(A,20);A->FindComponentByClass<UVTGEggKillReporterComponent>()->BeginPlay();return A;};
 auto* E1=MakeEgg();E1->SetFlags(RF_WasLoaded);
 E1->OnTakeAnyDamage.Broadcast(E1,1,nullptr,PC,Player);check(Encounter->GetMonsterEggKillCount()==0);
 SetHealth(E1,0);E1->OnTakeAnyDamage.Broadcast(E1,20,nullptr,PC,Player);check(Encounter->GetMonsterEggKillCount()==1);
 E1->OnTakeAnyDamage.Broadcast(E1,20,nullptr,PC,Player);check(Encounter->GetMonsterEggKillCount()==1);
 auto* Cleanup=MakeEgg();Cleanup->OnTakeAnyDamage.Broadcast(Cleanup,1,nullptr,PC,Player);SetHealth(Cleanup,0);Cleanup->OnTakeAnyDamage.Broadcast(Cleanup,100000,nullptr,nullptr,Other);check(Encounter->GetMonsterEggKillCount()==1);
 auto* E2=MakeEgg();SetHealth(E2,0);E2->OnTakeAnyDamage.Broadcast(E2,20,nullptr,nullptr,Weapon);check(Encounter->GetMonsterEggKillCount()==2);
 FVTGLevelProgressRecord Progress;LM->CaptureLevelProgress(Progress);check(Progress.Values.Contains(TEXT("L3_MonsterEggKillCount")) && Progress.Values.Contains(TEXT("L3_CountedEggIds")));
 FindFProperty<FIntProperty>(LM->GetClass(),TEXT("L3_MonsterEggKillCount"))->SetPropertyValue_InContainer(LM,0);LM->RestoreLevelProgress(Progress);check(Encounter->GetMonsterEggKillCount()==2);Encounter->RecordPlayerEggKill(E1);check(Encounter->GetMonsterEggKillCount()==2);
 auto* B=World->SpawnActor<AActor>(Boss->GeneratedClass,FTransform::Identity,Spawn);FindFProperty<FObjectPropertyBase>(LM->GetClass(),TEXT("L3_Boss"))->SetObjectPropertyValue_InContainer(LM,B);Encounter->BeginPlay();check(!B->CanBeDamaged());
 auto* AI=World->SpawnActor<AAIController>();auto* Tree=Load<UBehaviorTree>(TEXT("/Game/AI/BossAIToolkit/Blueprints/BehaviorTrees/BT_BossAI"));UBlackboardComponent* Blackboard=nullptr;check(AI->UseBlackboard(Tree->BlackboardAsset,Blackboard));
 UActorComponent* Behavior=nullptr;for(auto* C:B->GetComponents())if(FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))Behavior=C;check(Behavior);
 FindFProperty<FObjectPropertyBase>(Behavior->GetClass(),TEXT("ControllerRef"))->SetObjectPropertyValue_InContainer(Behavior,AI);FindFProperty<FObjectPropertyBase>(Behavior->GetClass(),TEXT("CharacterRef"))->SetObjectPropertyValue_InContainer(Behavior,B);
 Encounter->TickComponent(.1f,LEVELTICK_All,nullptr);check(!B->CanBeDamaged() && !Blackboard->GetValueAsBool(TEXT("InCombat")));
 check(Encounter->StartBossEncounter());Encounter->TickComponent(.1f,LEVELTICK_All,nullptr);
 check(B->CanBeDamaged() && Blackboard->GetValueAsBool(TEXT("InCombat")));check(FindFProperty<FBoolProperty>(LM->GetClass(),TEXT("L3_BossEncounterStarted"))->GetPropertyValue_InContainer(LM));
 for(auto* C:B->GetComponents())if(auto* P=FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath"))){auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(C));D->ProcessMulticastDelegate<UObject>(nullptr);D->ProcessMulticastDelegate<UObject>(nullptr);}
 check(FindFProperty<FBoolProperty>(LM->GetClass(),TEXT("L3_BossDefeated"))->GetPropertyValue_InContainer(LM));check(!Encounter->StartBossEncounter());
 UE_LOG(LogTemp,Display,TEXT("SEWER ENCOUNTER VERIFY PASS: saved map/graphs, dormant gate, death signal, nonlethal/player/weapon/cleanup/duplicate kills, save restore and saved-ID deduplication"));
 GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
}
}
UVTGSewerEncounterCommandlet::UVTGSewerEncounterCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSewerEncounterCommandlet::Main(const FString& Params){if(FParse::Param(*Params,TEXT("Apply")))SewerSetup::Apply();else SewerSetup::Verify();return 0;}
