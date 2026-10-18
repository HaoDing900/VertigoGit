#include "VTGStoryCombatLockCommandlet.h"
#include "Combat/VTGCombatPresentation.h"
#include "Combat/VTGCombatComponent.h"
#include "VTGLevelManagerBase.h"
#include "Components/WeaponBase_ISX.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StructOnScope.h"

UVTGStoryCombatLockCommandlet::UVTGStoryCombatLockCommandlet()
{ IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true; }
int32 UVTGStoryCombatLockCommandlet::Main(const FString&)
{
 TGuardValue<bool> Execution(GAllowActorScriptExecutionInEditor,true);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("StoryCombatLockTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Player=World->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn);check(Player);
 auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Player);
 auto* Manager=World->SpawnActor<AVTGLevelManagerBase>();
 auto* Presentation=Player->FindComponentByClass<UVTGPlayerPresentationComponent>();check(Presentation);
 auto Flag=[&](FName Name){auto* P=FindFProperty<FBoolProperty>(Player->GetClass(),Name);check(P);return P->GetPropertyValue_InContainer(Player);};
 auto Set=[&](FName Name,bool Value){auto* P=FindFProperty<FBoolProperty>(Player->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Player,Value);};
 auto* Equip=Player->FindFunction(TEXT("EquipMeleeWeapon"));check(Equip);
 auto EquipHammer=[&](){FStructOnScope Args(Equip);auto* P=FindFProperty<FByteProperty>(Equip,TEXT("CurrentMeleeWeapon"));check(P);P->SetPropertyValue_InContainer(Args.GetStructMemory(),1);Player->ProcessEvent(Equip,Args.GetStructMemory());};
 auto* Enum=FindFProperty<FByteProperty>(Player->GetClass(),TEXT("CurrentMeleeWeapon"));check(Enum);
 auto* Health=FindFProperty<FNumericProperty>(Player->GetClass(),TEXT("Health"));check(Health);
 if(Health->IsInteger())Health->SetIntPropertyValue(Health->ContainerPtrToValuePtr<void>(Player),uint64(100));
 else Health->SetFloatingPointPropertyValue(Health->ContainerPtrToValuePtr<void>(Player),100);
 Set(TEXT("IsDead?"),false);Set(TEXT("CanAttack?"),true);Set(TEXT("CanPunch?"),true);Set(TEXT("StoryMode?"),false);
 EquipHammer();check(Enum->GetPropertyValue_InContainer(Player)==1);
 auto* Ranged=NewObject<UWeaponBase_ISX>(Player);Player->AddInstanceComponent(Ranged);Ranged->RegisterComponent();
 auto* Item=NewObject<UVTGStoryLockTestItem>(Player);Ranged->SetEquippedItem(Item);
 Set(TEXT("InCombat"),true);Set(TEXT("HasWeaponInHand"),true);Set(TEXT("IsAutoFiring"),true);Set(TEXT("SaveAttack"),true);
 check(Manager->SetPlayerStoryCombatLock(true));check(Presentation->IsStoryCombatLocked());
 check(Enum->GetPropertyValue_InContainer(Player)==0 && !Ranged->GetEquippedItem() && Ranged->HidedItem==Item);
 check(!Flag(TEXT("InCombat")) && !Flag(TEXT("HasWeaponInHand")) && !Flag(TEXT("IsAutoFiring")) && !Flag(TEXT("SaveAttack")));
 check(!Presentation->ResolveCombatState(true,true));
 UE_LOG(LogTemp,Display,TEXT("PASS real Sa: hammer holstered via equipment event, InventorySystemX weapon hidden/preserved, combat/buffer stopped"));
 Presentation->SetStoryCombatLock(true);EquipHammer();Ranged->SetEquippedItem(Item);Set(TEXT("InCombat"),true);
 PC->UnPossess(); // Test lock enforcement independently from local viewport/HUD creation.
 static_cast<UActorComponent*>(Presentation)->TickComponent(.016,LEVELTICK_All,nullptr);PC->Possess(Player);
 check(Enum->GetPropertyValue_InContainer(Player)==0 && !Ranged->GetEquippedItem() && !Flag(TEXT("InCombat")));
 auto* Combat=NewObject<UVTGCombatComponent>(Player);Player->AddInstanceComponent(Combat);
 check(!Combat->CanDoAction(EVTGCombatAction::Attack) && !Combat->CanDoAction(EVTGCombatAction::Fire) && Combat->CanDoAction(EVTGCombatAction::Move));
 UE_LOG(LogTemp,Display,TEXT("PASS repeated enable preserves snapshot; re-equip/proximity suppressed; native combat gated without locking movement"));
 Set(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),true);
 check(Manager->SetPlayerStoryCombatLock(false));check(!Presentation->IsStoryCombatLocked());
 check(Flag(TEXT("CanAttack?")) && Flag(TEXT("CanPunch?")) && Flag(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")));
 check(Enum->GetPropertyValue_InContainer(Player)==0 && !Ranged->GetEquippedItem() && !Flag(TEXT("InCombat")));
 Set(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);check(Presentation->ResolveCombatState(true,false));
 Presentation->SetStoryCombatLock(false);check(Flag(TEXT("CanAttack?")));
 UE_LOG(LogTemp,Display,TEXT("PASS release restores attack permission; no auto-equip/combat; cinematic input flag untouched; normal combat resolution resumes"));
 Presentation->SetStoryCombatLock(true);Set(TEXT("IsDead?"),true);Presentation->SetStoryCombatLock(false);
 check(!Flag(TEXT("CanAttack?")) && Flag(TEXT("IsDead?")));
 UE_LOG(LogTemp,Display,TEXT("PASS release cannot revive player or restore dead player's attack permission"));
 GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return 0;
}
