#include "VTGL2FightRepairCommandlet.h"
#include "Save/VTGL2FightStateComponent.h"
#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveTypes.h"
#include "VTGGameInstanceBase.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneComponent.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

namespace L2Repair
{
 UEdGraphNode* Node(UEdGraph* G,const TCHAR* Name)
 {for(UEdGraphNode* N:G->Nodes)if(N->GetFName()==Name)return N;checkf(false,TEXT("Missing %s"),Name);return nullptr;}
 UEdGraphPin* P(UEdGraphNode* N,const TCHAR* Name){return N->FindPinChecked(Name);}
 void Link(UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
 void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){Link(P(A,AP),P(B,BP));}
 template<class T> T* Add(UEdGraph* G,int X,int Y)
 {auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;}
 UK2Node_CallFunction* Call(UEdGraph* G,UClass* C,const TCHAR* Fn,int X,int Y)
 {auto* N=Add<UK2Node_CallFunction>(G,X,Y);auto* F=C->FindFunctionByName(Fn);check(F);N->SetFromFunction(F);N->AllocateDefaultPins();return N;}
 UK2Node_VariableGet* Get(UEdGraph* G,const TCHAR* Name,int X,int Y)
 {auto* N=Add<UK2Node_VariableGet>(G,X,Y);N->VariableReference.SetSelfMember(Name);N->AllocateDefaultPins();return N;}
 bool Linked(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP)
 {return P(A,AP)->LinkedTo.Contains(P(B,BP));}
 void Assert(UEdGraph* G,const TCHAR* A,const TCHAR* AP,const TCHAR* B,const TCHAR* BP)
 {checkf(Linked(Node(G,A),AP,Node(G,B),BP),TEXT("Wrong link %s.%s -> %s.%s"),A,AP,B,BP);}
}

UVTGL2FightRepairCommandlet::UVTGL2FightRepairCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}

struct FVTGL2FightRegression
{
static void Run()
{
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("L2FightRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
 auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
 auto* Manager=World->SpawnActor<AActor>();auto* Client=World->SpawnActor<AActor>();
 auto* Root=NewObject<USceneComponent>(Client);Client->SetRootComponent(Root);Root->RegisterComponent();
 auto* Pawn=World->SpawnActor<ACharacter>();auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Pawn);
 if(!UGameplayStatics::GetPlayerController(World,0))World->AddController(PC);
 check(UGameplayStatics::GetPlayerPawn(World,0)==Pawn);
 auto* State=NewObject<UVTGL2FightStateComponent>(Manager);Manager->AddInstanceComponent(State);State->RegisterComponent();
 auto* Save=NewObject<UVTGSaveCoordinator>(GI);State->Coordinator=Save;
 Client->SetActorHiddenInGame(true);Client->SetActorLocation(FVector(1000,2000,3000));
 Pawn->SetActorLocation(FVector(0,5000,300));Pawn->GetCharacterMovement()->Velocity=FVector(300,400,0);
 State->CompleteAfterFight(Client);
 check(!Client->IsHidden() && Client->GetActorTransform().Equals(State->FemaleClientEndTransform));
 check(Pawn->GetActorTransform().Equals(State->PlayerEndTransform));
 check(Pawn->GetCharacterMovement()->Velocity.IsNearlyZero());
 check(Save->GetPersistentInt(TEXT("L2StreetFightPhase"))==2);
 check(Save->GetPersistentInt(TEXT("L2StreetFightAfterDialoguePending"))==1);
 // Simulate saved actor transforms overwriting the freshly created map actors.
 const FTransform SavedPawn(FRotator(0,35,0),FVector(321,432,543));Pawn->SetActorTransform(SavedPawn);
 Client->SetActorHiddenInGame(true);Client->SetActorLocation(FVector(1000,2000,3000));
 // A pending load must defer scene setup until OnSlotLoaded, after actor restoration.
 Save->SetPersistentInt(TEXT("L2StreetFightAfterDialoguePending"),0);
 Save->PendingLoad=NewObject<UVTGSaveGame>();
 State->ResumeAfterFight(Client);
 check(Client->IsHidden() && State->bWaitingForRestore);
 Save->OnSlotLoaded.Broadcast(1);
 check(!State->bWaitingForRestore);
 Save->PendingLoad=nullptr;
 check(!Client->IsHidden() && Client->GetActorTransform().Equals(State->FemaleClientEndTransform));
 check(Pawn->GetActorTransform().Equals(SavedPawn));
 // Repeated completion must not teleport a resumed player or restart finished dialogue.
 State->CompleteAfterFight(Client);
 check(Pawn->GetActorTransform().Equals(SavedPawn));
 check(Save->GetPersistentInt(TEXT("L2StreetFightAfterDialoguePending"))==0);
 auto* Snapshot=NewObject<UVTGSaveGame>();Snapshot->PersistentInts.Add(TEXT("L2StreetFightPhase"),Save->GetPersistentInt(TEXT("L2StreetFightPhase")));
 TArray<uint8> Bytes;check(UGameplayStatics::SaveGameToMemory(Snapshot,Bytes));
 auto* Loaded=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));check(Loaded->PersistentInts.FindChecked(TEXT("L2StreetFightPhase"))==2);
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 UE_LOG(LogTemp,Display,TEXT("L2 REGRESSION PASS: hidden NPC restored; moving pawn placed/stopped; real completion writes phase 2; pending load waits for OnSlotLoaded; saved pawn location preserved; duplicate completion safe; phase serialization. No user save files written."));
}
};

int32 UVTGL2FightRepairCommandlet::Main(const FString& Params)
{
 using namespace L2Repair;
 const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"));check(BP);
 UEdGraph* G=BP->UbergraphPages[0];
 if(!Verify)
 {
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/L2FightDiagnosis/BPLM_L2StreetBarFight.before_fix.uasset");
  check(!IFileManager::Get().FileExists(*Backup));check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
  USimpleConstructionScript* SCS=BP->SimpleConstructionScript;check(SCS);
  auto* Component=SCS->CreateNode(UVTGL2FightStateComponent::StaticClass(),TEXT("L2FightState"));SCS->AddNode(Component);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
  // Bind completion BEFORE Play. The old bind sat behind a post-Play Delay.
  auto* Bind=Node(G,TEXT("K2Node_AddDelegate_0"));auto* Rate=Node(G,TEXT("K2Node_CallFunction_113"));auto* Play=Node(G,TEXT("K2Node_CallFunction_1"));
  P(Bind,TEXT("execute"))->BreakAllPinLinks();P(Rate,TEXT("then"))->BreakAllPinLinks();
  Link(Rate,TEXT("then"),Bind,TEXT("execute"));Link(Bind,TEXT("then"),Play,TEXT("execute"));
  FBlueprintEditorUtils::RemoveNode(BP,Node(G,TEXT("K2Node_CallFunction_97")),true);
  // Initialization must precede combat entry, so it cannot reset the combat enemy afterwards.
  auto* Branch=Node(G,TEXT("K2Node_IfThenElse_7"));auto* Enter=Node(G,TEXT("K2Node_CallFunction_100"));auto* InitNPC=Node(G,TEXT("K2Node_CallFunction_102"));
  P(Branch,TEXT("then"))->BreakAllPinLinks();P(Enter,TEXT("then"))->BreakAllPinLinks();
  Link(Branch,TEXT("then"),InitNPC,TEXT("execute"));Link(InitNPC,TEXT("then"),Enter,TEXT("execute"));
  // Explicit handoff after Sequencer has restored pre-animation state.
  auto* Finish=Node(G,TEXT("K2Node_CustomEvent_16"));auto* Camera=Node(G,TEXT("K2Node_CallFunction_44"));
  auto* StateGet=Get(G,TEXT("L2FightState"),Finish->NodePosX,Finish->NodePosY+180);
  auto* ClientGet=Get(G,TEXT("L2_FemaleClient"),Finish->NodePosX,Finish->NodePosY+260);
  auto* Complete=Call(G,UVTGL2FightStateComponent::StaticClass(),TEXT("CompleteAfterFight"),Finish->NodePosX+240,Finish->NodePosY);
  Complete->Rename(TEXT("VTG_L2CompleteAfterFight"),G);Complete->NodeComment=TEXT("Restore final NPC/player state, persist phase 2 before dialogue");
  P(Finish,TEXT("then"))->BreakAllPinLinks();Link(Finish,TEXT("then"),Complete,TEXT("execute"));Link(Complete,TEXT("then"),Camera,TEXT("execute"));
  Link(StateGet,TEXT("L2FightState"),Complete,TEXT("self"));Link(ClientGet,TEXT("L2_FemaleClient"),Complete,TEXT("FemaleClient"));
  // The unused player teleport is replaced by the component's explicit gameplay handoff.
  FBlueprintEditorUtils::RemoveNode(BP,Node(G,TEXT("K2Node_CallFunction_60")),true);
  auto* AfterBranch=Add<UK2Node_IfThenElse>(G,Branch->NodePosX+250,Branch->NodePosY+380);AfterBranch->AllocateDefaultPins();AfterBranch->Rename(TEXT("VTG_L2AfterFightBranch"),G);
  auto* Equal=Call(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"),Branch->NodePosX,Branch->NodePosY+600);
  P(Equal,TEXT("B"))->DefaultValue=TEXT("2");Link(Node(G,TEXT("K2Node_CallFunction_99")),TEXT("ReturnValue"),Equal,TEXT("A"));Link(Equal,TEXT("ReturnValue"),AfterBranch,TEXT("Condition"));
  auto* Resume=Call(G,UVTGL2FightStateComponent::StaticClass(),TEXT("ResumeAfterFight"),Branch->NodePosX+510,Branch->NodePosY+380);Resume->Rename(TEXT("VTG_L2ResumeAfterFight"),G);
  Resume->NodeComment=TEXT("Wait for OnSlotLoaded; restore NPCs without replaying fight or moving saved player");
  auto* ResumeState=Get(G,TEXT("L2FightState"),Branch->NodePosX+250,Branch->NodePosY+700);
  auto* ResumeClient=Get(G,TEXT("L2_FemaleClient"),Branch->NodePosX+250,Branch->NodePosY+780);
  P(Branch,TEXT("else"))->BreakAllPinLinks();Link(Branch,TEXT("else"),AfterBranch,TEXT("execute"));
  Link(AfterBranch,TEXT("then"),Resume,TEXT("execute"));Link(AfterBranch,TEXT("else"),Node(G,TEXT("K2Node_CallFunction_63")),TEXT("execute"));
  Link(ResumeState,TEXT("L2FightState"),Resume,TEXT("self"));Link(ResumeClient,TEXT("L2_FemaleClient"),Resume,TEXT("FemaleClient"));
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);if(Log.NumErrors)return 2;
 Assert(G,TEXT("K2Node_CallFunction_113"),TEXT("then"),TEXT("K2Node_AddDelegate_0"),TEXT("execute"));
 Assert(G,TEXT("K2Node_AddDelegate_0"),TEXT("then"),TEXT("K2Node_CallFunction_1"),TEXT("execute"));
 Assert(G,TEXT("K2Node_CustomEvent_16"),TEXT("then"),TEXT("VTG_L2CompleteAfterFight"),TEXT("execute"));
 Assert(G,TEXT("VTG_L2CompleteAfterFight"),TEXT("then"),TEXT("K2Node_CallFunction_44"),TEXT("execute"));
 Assert(G,TEXT("K2Node_IfThenElse_7"),TEXT("then"),TEXT("K2Node_CallFunction_102"),TEXT("execute"));
 Assert(G,TEXT("K2Node_CallFunction_102"),TEXT("then"),TEXT("K2Node_CallFunction_100"),TEXT("execute"));
 Assert(G,TEXT("VTG_L2AfterFightBranch"),TEXT("then"),TEXT("VTG_L2ResumeAfterFight"),TEXT("execute"));
 Assert(G,TEXT("VTG_L2AfterFightBranch"),TEXT("else"),TEXT("K2Node_CallFunction_63"),TEXT("execute"));
 check(FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("L2FightState")));
 FVTGL2FightRegression::Run();
 if(!Verify)
 {
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
  check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
 }
 UE_LOG(LogTemp,Display,TEXT("L2 BLUEPRINT PASS: compile clean, phase 2 resume branch, explicit completion handoff, NPC init before combat, OnFinished bound before Play. %s"),Verify?TEXT("Read-back verification"):TEXT("Saved"));
 return 0;
}
