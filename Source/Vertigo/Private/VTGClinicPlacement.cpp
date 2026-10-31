#include "VTGClinicPlacement.h"
#include "NarrativeComponent.h"
#include "NarrativeUIInputComponent.h"
#include "Dialogue.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UVTGClinicPlacementState::Place(ACharacter* Player,AActor* FacingTarget,const FVector& Location,float FallbackYaw)
{
 if(!IsValid(Player))return;
 if(!bHolding)
 {
  Character=Player;Controller=Cast<APlayerController>(Player->GetController());
  auto* Move=Player->GetCharacterMovement();bOrientToMovement=Move->bOrientRotationToMovement;bControllerDesired=Move->bUseControllerDesiredRotation;
  bControllerYaw=Player->bUseControllerRotationYaw;bControllerPitch=Player->bUseControllerRotationPitch;bControllerRoll=Player->bUseControllerRotationRoll;
  Narrative=Player->FindComponentByClass<UNarrativeComponent>();bHolding=true;
  if(Controller.IsValid())if(auto* Input=UNarrativeUIInputComponent::GetOrCreate(Controller.Get()))Input->Acquire(this);
  if(Narrative.IsValid())Narrative->OnDialogueBegan.AddUniqueDynamic(this,&UVTGClinicPlacementState::DialogueBegan);
 }
 auto* Move=Player->GetCharacterMovement();Move->StopMovementImmediately();Player->ConsumeMovementInputVector();
 Move->bOrientRotationToMovement=false;Move->bUseControllerDesiredRotation=false;
 Player->bUseControllerRotationYaw=false;Player->bUseControllerRotationPitch=false;Player->bUseControllerRotationRoll=false;
 FVector Facing=IsValid(FacingTarget)?FacingTarget->GetActorLocation()-Location:FVector::ZeroVector;Facing.Z=0.f;
 const float Yaw=Facing.IsNearlyZero()?FallbackYaw:Facing.Rotation().Yaw;
 if(auto* C=Player->GetController()){auto View=C->GetControlRotation();View.Yaw=Yaw;C->SetControlRotation(View);}
 Player->SetActorLocationAndRotation(Location,FRotator(0.f,Yaw,0.f),false,nullptr,ETeleportType::TeleportPhysics);
 // Repeated placement refreshes the deadline without stacking input-lock increments or overwriting the snapshot.
 GetWorld()->GetTimerManager().ClearTimer(Handoff);
 GetWorld()->GetTimerManager().SetTimer(Deadline,this,&UVTGClinicPlacementState::Release,3.f,false);
}
void UVTGClinicPlacementState::DialogueBegan(UDialogue* Dialogue)
{
 if(!bHolding || !Dialogue || Dialogue->bFreeMovement || !GetWorld())return;
 // OnDialogueBegan is broadcast BEFORE UDialogue::OnBeginDialogue acquires its own UI input lease.
 // Wait until the next tick so removing this request cannot create an unlocked handoff frame.
 GetWorld()->GetTimerManager().ClearTimer(Handoff);
 Handoff=GetWorld()->GetTimerManager().SetTimerForNextTick(this,&UVTGClinicPlacementState::Release);
}
void UVTGClinicPlacementState::Release()
{
 if(!bHolding)return;bHolding=false;
 if(GetWorld()){GetWorld()->GetTimerManager().ClearTimer(Deadline);GetWorld()->GetTimerManager().ClearTimer(Handoff);}
 if(Narrative.IsValid())Narrative->OnDialogueBegan.RemoveDynamic(this,&UVTGClinicPlacementState::DialogueBegan);
 if(Character.IsValid())
 {
  auto* Player=Character.Get();auto* Move=Player->GetCharacterMovement();Move->StopMovementImmediately();Player->ConsumeMovementInputVector();
  // Keep restored controller-facing modes aligned with the final scene heading.
  if((bControllerYaw || bControllerDesired) && Player->GetController()){auto View=Player->GetController()->GetControlRotation();View.Yaw=Player->GetActorRotation().Yaw;Player->GetController()->SetControlRotation(View);}
  Move->bOrientRotationToMovement=bOrientToMovement;Move->bUseControllerDesiredRotation=bControllerDesired;
  Player->bUseControllerRotationYaw=bControllerYaw;Player->bUseControllerRotationPitch=bControllerPitch;Player->bUseControllerRotationRoll=bControllerRoll;
 }
 if(Controller.IsValid())if(auto* Input=UNarrativeUIInputComponent::Find(Controller.Get()))Input->Release(this);
 Character.Reset();Controller.Reset();Narrative.Reset();
}
void UVTGClinicPlacementState::EndPlay(const EEndPlayReason::Type Reason){Release();Super::EndPlay(Reason);}
void UVTGClinicPlacementLibrary::PlaceClinicPlayer(ACharacter* Player,AActor* FacingTarget,FVector Location,float FallbackYaw)
{
 if(!IsValid(Player))return;
 auto* State=Player->FindComponentByClass<UVTGClinicPlacementState>();
 if(!State){State=NewObject<UVTGClinicPlacementState>(Player);Player->AddInstanceComponent(State);State->RegisterComponent();}
 State->Place(Player,FacingTarget,Location,FallbackYaw);
}
