#include "VTGClinicButtonFlowTestComponent.h"
#include "VTGClinicFix.h"
#include "VTGLevelManagerBase.h"
#include "Save/VTGSaveTypes.h"
#include "Dialogue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "UObject/UnrealType.h"
#include "Engine/LatentActionManager.h"

bool VTGTestClinicButtonFlow()
{
 FEditorScriptExecutionGuard Guard;
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ClinicButtonFlowTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
 auto* MC=LoadClass<AVTGLevelManagerBase>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic.BPLM_Clinic_C"));auto* BC=LoadClass<AActor>(nullptr,TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision.BP_0ISXInteractiveCollision_C"));check(MC && BC);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Manager=W->SpawnActor<AVTGLevelManagerBase>(MC,FTransform::Identity,Spawn);check(Manager);
 auto Obj=[&](AActor* A,const TCHAR* N,UObject* V){auto* P=FindFProperty<FObjectPropertyBase>(A->GetClass(),N);check(P);P->SetObjectPropertyValue_InContainer(A,V);};
 auto SetBool=[&](const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(MC,N);check(P);P->SetPropertyValue_InContainer(Manager,V);};
 auto GetBool=[&](const TCHAR* N){auto* P=FindFProperty<FBoolProperty>(MC,N);check(P);return P->GetPropertyValue_InContainer(Manager);};
 auto* Pawn=W->SpawnActor<ACharacter>();Obj(Manager,TEXT("Player Pawn"),Pawn);
 auto* UlisesClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/Characters/Ulises/BP_Ulises_NPC.BP_Ulises_NPC_C"));check(UlisesClass);auto* Ulises=W->SpawnActor<ACharacter>(UlisesClass,FTransform::Identity,Spawn);check(Ulises);Obj(Manager,TEXT("Ulises"),Ulises);
 auto* Talk=W->SpawnActor<AActor>(BC,FTransform::Identity,Spawn);check(Talk);Obj(Manager,TEXT("ISXColi_L5UlisesConvAfterExploration"),Talk);
 auto* NC=NewObject<UVTGClinicButtonFlowTestComponent>(Pawn);Pawn->AddInstanceComponent(NC);NC->RegisterComponent();Obj(Manager,TEXT("Narrative"),NC);
 auto* D=NewObject<UDialogue>(W);D->OwningComp=NC;auto* Reminder=NewObject<UVTGClinicButtonEvent>(D);
 auto Call=[&](const TCHAR* N){Manager->ProcessEvent(Manager->FindFunctionChecked(N),nullptr);};
 auto Advance=[&](){for(int I=0;I<45;++I){++GFrameCounter;W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Manager,1.f/60.f);}};
 auto ButtonValue=[&](){return FindFProperty<FObjectPropertyBase>(MC,TEXT("ISXColi_L5Button"))->GetObjectPropertyValue_InContainer(Manager);};
 for(bool RejectReaction:{false,true})
 {
  NC->Starts.Reset();NC->AcceptDialogue=true;SetBool(TEXT("L5_HadPressedButton"),false);SetBool(TEXT("L5_AfterExplore_UlisesHadTalkedCaseNAskToPressButton"),false);
  auto* Button=W->SpawnActor<AActor>(BC,FTransform::Identity,Spawn);check(Button);Obj(Manager,TEXT("ISXColi_L5Button"),Button);
  Button->ProcessEvent(Button->FindFunctionChecked(TEXT("DisableInteract")),nullptr);Reminder->ExecuteEvent(Pawn,nullptr,NC);
  check(GetBool(TEXT("L5_AfterExplore_UlisesHadTalkedCaseNAskToPressButton")));check(FindFProperty<FBoolProperty>(BC,TEXT("CanInteract?"))->GetPropertyValue_InContainer(Button));
  for(int I=0;I<3;++I){Call(TEXT("L5_ClinicUlisesConvAfterExplore"));check(NC->Starts.Last()==TEXT("L5Clinic_Still_DialogueNode_NPC_232"));check(IsValid(Button) && !GetBool(TEXT("L5_HadPressedButton")));}
  UE_LOG(LogTemp,Display,TEXT("BUTTON FLOW TEST PASS: reaching NPC_232 enables button; repeated Ulises requests remain NPC_232 before interaction"));
  NC->AcceptDialogue=!RejectReaction;Call(TEXT("L5_Button"));check(GetBool(TEXT("L5_HadPressedButton")));check(!IsValid(Button) && ButtonValue()==nullptr);
  const int Count=NC->Starts.Num();Call(TEXT("L5_Button"));Call(TEXT("L5_Button"));check(NC->Starts.Num()==Count && GetBool(TEXT("L5_HadPressedButton")));
  Call(TEXT("L5_ShowUlisesInteractionIcon"));check(GetBool(TEXT("L5_HadPressedButton")));Reminder->ExecuteEvent(Pawn,nullptr,NC);check(ButtonValue()==nullptr);
  NC->AcceptDialogue=true;Call(TEXT("L5_ClinicUlisesConvAfterExplore"));Advance();check(NC->Starts.Last()==TEXT("L5Clinic_Still_DialogueNode_NPC_233"));
  UE_LOG(LogTemp,Display,TEXT("BUTTON FLOW TEST PASS: pressed state survives icon refresh; actor destroyed and reference cleared; duplicate callbacks ignored; actual BPLM routes to NPC_233 (reaction rejected=%d)"),RejectReaction);
 }
 FVTGLevelProgressRecord Progress;Manager->CaptureLevelProgress(Progress);check(Progress.Values.Contains(TEXT("L5_HadPressedButton")));
 SetBool(TEXT("L5_HadPressedButton"),false);Manager->RestoreLevelProgress(Progress);Call(TEXT("L5_ShowUlisesInteractionIcon"));check(GetBool(TEXT("L5_HadPressedButton")));
 UE_LOG(LogTemp,Display,TEXT("BUTTON FLOW TEST PASS: pressed state persists through level progress serialization and icon refresh"));
 W->GetLatentActionManager().RemoveActionsForObject(Manager);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
