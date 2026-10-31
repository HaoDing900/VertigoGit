#include "VTGClinicPlacement.h"
#include "NarrativeComponent.h"
#include "NarrativeUIInputComponent.h"
#include "Dialogue.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

bool VTGTestClinicPlacement()
{
 FEditorScriptExecutionGuard Guard;
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ClinicPlacementTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* PlayerClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(PlayerClass);auto* Player=W->SpawnActor<ACharacter>(PlayerClass,FTransform::Identity,Spawn);check(Player);
 auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);PC->Possess(Player);if(!PC->PlayerCameraManager){PC->PlayerCameraManager=W->SpawnActor<APlayerCameraManager>();PC->PlayerCameraManager->InitializeFor(PC);}
 auto* NC=Player->FindComponentByClass<UNarrativeComponent>();check(NC);auto* Input=UNarrativeUIInputComponent::GetOrCreate(PC);check(Input);
 auto* MC=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic.BPLM_Clinic_C"));check(MC);auto* Manager=W->SpawnActor<AActor>(MC,FTransform::Identity,Spawn);
 auto* UC=LoadClass<ACharacter>(nullptr,TEXT("/Game/Characters/Ulises/BP_Ulises_NPC.BP_Ulises_NPC_C"));check(UC);auto* Ulises=W->SpawnActor<ACharacter>(UC,FTransform::Identity,Spawn);check(Ulises);
 FindFProperty<FObjectPropertyBase>(MC,TEXT("Player Pawn"))->SetObjectPropertyValue_InContainer(Manager,Player);FindFProperty<FObjectPropertyBase>(MC,TEXT("Ulises"))->SetObjectPropertyValue_InContainer(Manager,Ulises);
 auto* Move=Player->GetCharacterMovement();
 auto Tick=[&](int N){for(int I=0;I<N;++I){++GFrameCounter;W->GetTimerManager().Tick(1.f/60.f);}};
 auto FacesTarget=[&](){auto Dir=Ulises->GetActorLocation()-Player->GetActorLocation();Dir.Z=0;return FVector::DotProduct(Player->GetActorForwardVector(),Dir.GetSafeNormal())>.9999;};
 for(int Case=0;Case<4;++Case)
 {
  const bool Orient=Case!=1;const bool Desired=Case==1;const bool ControllerYaw=Case==2;
  Move->bOrientRotationToMovement=Orient;Move->bUseControllerDesiredRotation=Desired;Player->bUseControllerRotationYaw=ControllerYaw;
  Player->SetActorLocationAndRotation(FVector(1000,1000,250),FRotator(0,Case*90.f,0));Ulises->SetActorLocation(FVector(999,-999,250));PC->SetControlRotation(FRotator(-12,Case*90.f,0));
  Move->Velocity=FVector(500,150,0);Player->AddMovementInput(FVector::ForwardVector,1.f,true);
  if(Case==1)PC->SetIgnoreMoveInput(true);
  auto* Modal=NewObject<UDialogue>(W);if(Case==2)Input->Acquire(Modal);
  Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("L5_ClinicRoom_Loc_SaUlises")),nullptr);
  check(Player->GetActorLocation().Equals(FVector(23.264730,-234.401798,249.394131),.01f));check(FacesTarget());
  check(Move->Velocity.IsNearlyZero() && Player->GetPendingMovementInputVector().IsNearlyZero());check(PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());check(!Move->bOrientRotationToMovement && !Move->bUseControllerDesiredRotation && !Player->bUseControllerRotationYaw);
  // Holding the movement key and an old controller heading during the one-second entrance gap cannot rotate the pawn.
  for(int I=0;I<65;++I){Player->AddMovementInput(FVector::ForwardVector);PC->SetControlRotation(FRotator(0,Case*90.f,0));Player->FaceRotation(PC->GetControlRotation(),1.f/60.f);Move->PhysicsRotation(1.f/60.f);Tick(1);check(FacesTarget());}
  if(Case<3)
  {
   auto* D=NewObject<UDialogue>(W);D->OwningComp=NC;D->OwningController=PC;D->OwningPawn=Player;D->bFreeMovement=false;NC->CurrentDialogue=D;
   NC->OnDialogueBegan.Broadcast(D);check(PC->IsMoveInputIgnored());Input->Acquire(D);Tick(2);
   check(PC->IsMoveInputIgnored() && Input->HasOwners());check(Move->bOrientRotationToMovement==Orient && Move->bUseControllerDesiredRotation==Desired && Player->bUseControllerRotationYaw==ControllerYaw);
   Input->Release(D);NC->CurrentDialogue=nullptr;
   if(Case==1){check(PC->IsMoveInputIgnored());PC->SetIgnoreMoveInput(false);}
   if(Case==2){check(PC->IsMoveInputIgnored());Input->Release(Modal);}
  }
  else
  {
   // A rejected/missing dialogue must never leave the player permanently locked.
   Tick(135);check(Move->bOrientRotationToMovement==Orient && Move->bUseControllerDesiredRotation==Desired && Player->bUseControllerRotationYaw==ControllerYaw);
  }
  check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored());check(FacesTarget());
  UE_LOG(LogTemp,Display,TEXT("CLINIC PLACEMENT TEST PASS case=%d yaw=%.3f: actual BPLM places NPC first; pending motion cleared; 65 held-input frames stable; rotation modes and input ownership restored"),Case,Player->GetActorRotation().Yaw);
 }
 // Repeated placement uses one lease and preserves the first rotation snapshot.
 Move->bOrientRotationToMovement=true;UVTGClinicPlacementLibrary::PlaceClinicPlayer(Player,Ulises,Player->GetActorLocation());UVTGClinicPlacementLibrary::PlaceClinicPlayer(Player,Ulises,Player->GetActorLocation());Tick(195);check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && Move->bOrientRotationToMovement);
 UE_LOG(LogTemp,Display,TEXT("CLINIC PLACEMENT TEST PASS: repeated placement does not stack input locks; fail-safe releases"));
 GI->RemoveLocalPlayer(Local);Local->PlayerController=nullptr;W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
