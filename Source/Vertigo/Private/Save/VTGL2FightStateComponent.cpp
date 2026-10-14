#include "Save/VTGL2FightStateComponent.h"
#include "Save/VTGSaveCoordinator.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "UObject/UnrealType.h"

namespace L2FightState
{
 const FName PhaseKey(TEXT("L2StreetFightPhase"));
 const FName DialoguePendingKey(TEXT("L2StreetFightAfterDialoguePending"));
 void SetBool(UObject* Object, const TCHAR* Name, bool Value)
 {
  if (Object) if (auto* P=FindFProperty<FBoolProperty>(Object->GetClass(),Name))
   P->SetPropertyValue_InContainer(Object,Value);
 }
 AActor* ActorProperty(UObject* Object,const TCHAR* Name)
 {
  if (Object) if (auto* P=FindFProperty<FObjectPropertyBase>(Object->GetClass(),Name))
   return Cast<AActor>(P->GetObjectPropertyValue_InContainer(Object));
  return nullptr;
 }
}

UVTGL2FightStateComponent::UVTGL2FightStateComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 FemaleClientEndTransform=FTransform(FRotator(0,-171.954788,0),FVector(-682.458618,2258.969482,242));
 // The existing BP's authored gameplay position; use the Sequence's final facing, unit scale.
 PlayerEndTransform=FTransform(FRotator(0,75.756935,0),FVector(-402,2119,251));
}

void UVTGL2FightStateComponent::BindNarrative()
{
 if (!Coordinator && GetWorld() && GetWorld()->GetGameInstance())
  Coordinator=GetWorld()->GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>();
 if (auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
  Narrative=Pawn->FindComponentByClass<UNarrativeComponent>();
 if (Narrative)
 {
  Narrative->OnDialogueBegan.AddUniqueDynamic(this,&UVTGL2FightStateComponent::OnDialogueBegan);
  Narrative->OnDialogueFinished.AddUniqueDynamic(this,&UVTGL2FightStateComponent::OnDialogueFinished);
 }
}

void UVTGL2FightStateComponent::ApplyAfterFightState(bool bPlacePlayer)
{
 using namespace L2FightState;
 if (IsValid(Client))
 {
  Client->SetActorHiddenInGame(false);
  Client->SetActorTransform(FemaleClientEndTransform,false,nullptr,ETeleportType::TeleportPhysics);
 }
 for (const TCHAR* Name : {TEXT("L2_EnmBarFighter"),TEXT("FightAreaCollision")})
  if (auto* Actor=ActorProperty(GetOwner(),Name); IsValid(Actor)) Actor->Destroy();
 if (auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
 {
  SetBool(Pawn,TEXT("InCombat"),false);
  SetBool(Pawn,TEXT("GunOrFistEquipped"),false);
  SetBool(Pawn,TEXT("StoryMode?"),true);
  if (bPlacePlayer)
  {
   if (auto* Character=Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
   Pawn->SetActorTransform(PlayerEndTransform,false,nullptr,ETeleportType::TeleportPhysics);
   if (auto* PC=Cast<APlayerController>(Pawn->GetController())) PC->SetControlRotation(PlayerEndTransform.Rotator());
  }
 }
}

void UVTGL2FightStateComponent::CompleteAfterFight(AActor* FemaleClient)
{
 if (bCompletionApplied) return;
 bCompletionApplied=true;
 Client=FemaleClient;
 BindNarrative();
 ApplyAfterFightState(true);
 if (Coordinator)
 {
  Coordinator->SetPersistentInt(L2FightState::PhaseKey,2);
  Coordinator->SetPersistentInt(L2FightState::DialoguePendingKey,1);
 }
 // Keep the existing autosave cadence: subsequent saves capture the completed fight.
}

void UVTGL2FightStateComponent::ResumeAfterFight(AActor* FemaleClient)
{
 Client=FemaleClient;
 BindNarrative();
 if (Coordinator && Coordinator->HasPendingWorldRestore())
 {
  bWaitingForRestore=true;
  Coordinator->OnSlotLoaded.AddUniqueDynamic(this,&UVTGL2FightStateComponent::OnSlotLoaded);
 }
 else FinishResume();
}

void UVTGL2FightStateComponent::OnSlotLoaded(int32 Slot)
{
 if (!bWaitingForRestore) return;
 bWaitingForRestore=false;
 if (Coordinator) Coordinator->OnSlotLoaded.RemoveDynamic(this,&UVTGL2FightStateComponent::OnSlotLoaded);
 FinishResume();
}

void UVTGL2FightStateComponent::FinishResume()
{
 BindNarrative();
 // ApplyWorldState has restored the saved pawn location. Preserve it on load.
 ApplyAfterFightState(false);
 if (Coordinator && Coordinator->GetPersistentInt(L2FightState::DialoguePendingKey,1)!=0)
 {
  if (auto* Fn=GetOwner()->FindFunction(TEXT("AfterFightDialogue"))) GetOwner()->ProcessEvent(Fn,nullptr);
 }
 else if (auto* PC=UGameplayStatics::GetPlayerController(this,0))
 {
  PC->ResetIgnoreInputFlags();
  if (auto* Pawn=PC->GetPawn())
  {
   Pawn->EnableInput(PC);
   PC->SetViewTarget(Pawn);
   L2FightState::SetBool(Pawn,TEXT("IsInCinematicCutscene_LockInput"),false);
   if (auto* Character=Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  }
 }
}

void UVTGL2FightStateComponent::OnDialogueBegan(UDialogue* Dialogue)
{
 if (Coordinator && Coordinator->GetPersistentInt(L2FightState::PhaseKey)==2
  && Coordinator->GetPersistentInt(L2FightState::DialoguePendingKey,0)!=0 && Dialogue
  && Dialogue->GetClass()->GetPathName()==TEXT("/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Street_NPC.L2_Street_NPC_C"))
  AfterFightDialogue=Dialogue;
}

void UVTGL2FightStateComponent::OnDialogueFinished(UDialogue* Dialogue,bool bStartingNewDialogue)
{
 if (Dialogue && Dialogue==AfterFightDialogue)
 {
  if (Coordinator) Coordinator->SetPersistentInt(L2FightState::DialoguePendingKey,0);
  AfterFightDialogue=nullptr;
 }
}

void UVTGL2FightStateComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if (Coordinator) Coordinator->OnSlotLoaded.RemoveDynamic(this,&UVTGL2FightStateComponent::OnSlotLoaded);
 if (Narrative)
 {
  Narrative->OnDialogueBegan.RemoveDynamic(this,&UVTGL2FightStateComponent::OnDialogueBegan);
  Narrative->OnDialogueFinished.RemoveDynamic(this,&UVTGL2FightStateComponent::OnDialogueFinished);
 }
 Super::EndPlay(Reason);
}
