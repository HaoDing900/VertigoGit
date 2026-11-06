#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "Combat/VTGFistWindComponent.h"
#include "Combat/VTGSewerEncounterComponent.h"
#include "Combat/VTGSewerBossPresentation.h"
#include "UI/VTGBossHealthBarComponent.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "NiagaraComponent.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

extern void VTGApplyCombatSewerPolish(bool Verify);
bool VTGVerifyCombatSewerPolish()
{
 VTGApplyCombatSewerPolish(true);
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 auto Expect=[](bool OK,const TCHAR* Label){checkf(OK,TEXT("%s"),Label);UE_LOG(LogTemp,Display,TEXT("COMBAT_SEWER_TEST PASS %s"),Label);};
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CombatSewerPolishTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* SaBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(SaBP);
 auto* Sa=W->SpawnActor<ACharacter>(SaBP->GeneratedClass,FTransform::Identity,Spawn);check(Sa);
 auto* PC=W->SpawnActor<APlayerController>();PC->Possess(Sa);W->AddController(PC);
 auto Flag=[&](FName Name,bool Value){auto* P=FindFProperty<FBoolProperty>(Sa->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Sa,Value);};
 for(FName Name:{TEXT("InCombat"),TEXT("IsAttacking")})Flag(Name,true);
 for(FName Name:{TEXT("StoryMode?"),TEXT("IsDead?"),TEXT("HidePlayerNCam"),TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),TEXT("HasWeaponInHand")})Flag(Name,false);
 auto* Weapon=FindFProperty<FByteProperty>(Sa->GetClass(),TEXT("CurrentMeleeWeapon"));check(Weapon);Weapon->SetPropertyValue_InContainer(Sa,0);
 auto* Mesh=Sa->GetMesh();Mesh->InitAnim(true);auto* Anim=CastChecked<UVTGCameraLookAnimInstance>(Mesh->GetAnimInstance());Anim->bCameraLookEnabled=false;
 if(auto* P=FindFProperty<FObjectPropertyBase>(Anim->GetClass(),TEXT("As Sa")))P->SetObjectPropertyValue_InContainer(Anim,Sa);
 auto* Wind=Sa->FindComponentByClass<UVTGFistWindComponent>();check(Wind && !Wind->PunchMontages.IsEmpty());auto* Montage=Wind->PunchMontages[0].Get();check(Anim->Montage_Play(Montage)>0);Anim->Montage_SetPosition(Montage,.3f);Anim->Montage_SetPlayRate(Montage,0);
 const auto ActorBefore=Sa->GetActorTransform();const auto CameraBefore=PC->GetControlRotation();
 auto Pose=[&](float Degrees){Anim->PunchDownDegrees=Degrees;for(int I=0;I<8;++I){Mesh->TickAnimation(.1f,false);Mesh->RefreshBoneTransforms();}return Mesh->GetSocketQuaternion(TEXT("spine_02_x"));};
 const FQuat Normal=Pose(0);const FQuat Down=Pose(15);const FQuat Delta=Down*Normal.Inverse();const FQuat Expected=Sa->GetActorQuat()*FRotator(-15,0,0).Quaternion()*Sa->GetActorQuat().Inverse();
 UE_LOG(LogTemp,Display,TEXT("PUNCH_POSE alpha=%.3f delta=%s errorDegrees=%.3f"),Anim->PunchDownAlpha,*Delta.Rotator().ToString(),FMath::RadiansToDegrees(Delta.AngularDistance(Expected)));
 Expect(Anim->PunchDownAlpha>.99 && FMath::RadiansToDegrees(Delta.AngularDistance(Expected))<.5f,TEXT("Evaluated ABP applies an actual 15-degree downward actor-space pitch during a punch"));
 Expect(Sa->GetActorTransform().Equals(ActorBefore) && PC->GetControlRotation().Equals(CameraBefore),TEXT("Punch correction leaves actor and camera transforms unchanged"));
 Weapon->SetPropertyValue_InContainer(Sa,1);Anim->NativeUpdateAnimation(.2f);Expect(Anim->PunchDownAlpha==0,TEXT("Weapon attacks receive no extra fist correction"));
 Weapon->SetPropertyValue_InContainer(Sa,0);Flag(TEXT("IsAttacking"),false);Anim->NativeUpdateAnimation(.2f);Expect(Anim->PunchDownAlpha==0,TEXT("Stopping punch removes correction"));
 Flag(TEXT("InCombat"),false);Anim->NativeUpdateAnimation(.2f);Expect(Anim->PunchDownAlpha==0,TEXT("Outside combat no punch offset remains"));Anim->Montage_Stop(0,Montage);
 auto* Narrative=Sa->FindComponentByClass<UNarrativeComponent>();check(Narrative);
 auto* Class=LoadClass<UDialogue>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Freemove.L3Sewer_Freemove_C"));check(Class);
 FDialoguePlayParams Params;Params.StartFromID=TEXT("L3Sewer_Freemove_DialogueNode_NPC_2");Expect(Narrative->BeginDialogue(Class,Params),TEXT("Real Freemove dialogue starts at the reported NPC_2"));
 auto* D=Narrative->GetCurrentDialogue();check(D);
 auto Node=[&](){auto* P=FindFProperty<FObjectPropertyBase>(D->GetClass(),TEXT("CurrentNode"));check(P);return Cast<UDialogueNode>(P->GetObjectPropertyValue_InContainer(D));};
 auto Finish=[&](){D->ProcessEvent(D->FindFunctionChecked(TEXT("FinishNPCDialogue")),nullptr);};check(Node() && Node()->GetID()==Params.StartFromID);
 D->bAlwaysAutoAdvance=false;D->SetAutoAdvance(false);Finish();
 Expect(D->IsWaitingForManualAdvance() && Node()->GetID()==Params.StartFromID,TEXT("Reproduced old failure: Auto OFF leaves NPC_2 waiting for unavailable manual continue"));
 D->bAlwaysAutoAdvance=true;D->SetAutoAdvance(false);Finish();
 Expect(!D->IsWaitingForManualAdvance() && Node() && Node()->GetID()==TEXT("L3Sewer_Freemove_DialogueNode_NPC_3"),TEXT("Freemove override advances NPC_2 to NPC_3 even with global Auto OFF"));
 Finish();Expect(!Narrative->GetCurrentDialogue(),TEXT("Freemove chain finishes naturally and exits instead of remaining stuck"));
 auto* Story=NewObject<UDialogue>();Story->SetAutoAdvance(false);Expect(!Story->IsAutoAdvanceEnabled(),TEXT("Ordinary story dialogues retain manual mode"));
 auto* ManagerBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"));auto* BossBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer"));check(ManagerBP && BossBP);
 auto* Manager=W->SpawnActor<AActor>(ManagerBP->GeneratedClass,FTransform::Identity,Spawn);auto* Boss=W->SpawnActor<AActor>(BossBP->GeneratedClass,FTransform::Identity,Spawn);check(Manager && Boss);
 auto* BossRef=FindFProperty<FObjectPropertyBase>(Manager->GetClass(),TEXT("L3_Boss"));check(BossRef);BossRef->SetObjectPropertyValue_InContainer(Manager,Boss);
 auto* Encounter=Manager->FindComponentByClass<UVTGSewerEncounterComponent>();auto* HUD=Boss->FindComponentByClass<UVTGBossHealthBarComponent>();check(Encounter && HUD);
 Expect(Encounter->StartBossEncounter(true) && !HUD->IsPresentationAllowed(),TEXT("Boss encounter hides the health HUD before combat/intro dialogue can overlap"));
 auto* Current=FindFProperty<FObjectPropertyBase>(Narrative->GetClass(),TEXT("CurrentDialogue"));check(Current);
 for(FName Entry:{TEXT("L3Sewer_Still_DialogueNode_NPC_19"),TEXT("L3Sewer_Still_DialogueNode_NPC_20")})
 {
  HUD->SetPresentationAllowed(false);auto* Intro=NewObject<UDialogue>(Narrative);Intro->OwningComp=Narrative;Current->SetObjectPropertyValue_InContainer(Narrative,Intro);
  Encounter->WaitForBossIntroDialogue(Narrative,true);Expect(!HUD->IsPresentationAllowed(),TEXT("Each intro branch keeps HUD hidden throughout its dialogue"));
  Narrative->OnDialogueFinished.Broadcast(NewObject<UDialogue>(Narrative),false);Expect(!HUD->IsPresentationAllowed(),TEXT("An unrelated dialogue finish cannot release the boss HUD"));
  Narrative->ForceExitDialogue();Expect(HUD->IsPresentationAllowed(),TEXT("Matching intro finish/interruption releases the boss HUD normally"));
 }
 HUD->SetPresentationAllowed(false);Encounter->WaitForBossIntroDialogue(Narrative,false);Expect(HUD->IsPresentationAllowed(),TEXT("Failed dialogue start cannot leave the boss health bar permanently hidden"));
 auto* ProjectileBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_AcidProjectile"));check(ProjectileBP);
 auto* Projectile=W->SpawnActor<AActor>(ProjectileBP->GeneratedClass,FTransform(FVector(0,0,5000)),Spawn);auto* Flight=Projectile->FindComponentByClass<UVTGAcidFlightComponent>();auto* Sphere=Projectile->FindComponentByClass<USphereComponent>();auto* Visual=Projectile->FindComponentByClass<UNiagaraComponent>();check(Flight && Sphere && Visual);
 Expect(Flight->MaxTravelDistance==3600,TEXT("Acid projectile flight range doubled from 1800 to 3600 cm"));
 Expect(Visual->GetRelativeScale3D().Equals(FVector(2.4)) && Sphere->GetUnscaledSphereRadius()==48,TEXT("Acid visual and collision dimensions both doubled"));
 UStaticMeshComponent* Core=nullptr;for(auto* C:Projectile->GetComponents())if(C->GetFName()==TEXT("VisibleAcidCore"))Core=Cast<UStaticMeshComponent>(C);
 Expect(Core && Core->GetComponentScale().Equals(FVector(.48),.001) && FMath::IsNearlyEqual(Sphere->GetScaledSphereRadius(),48.f,.01f),TEXT("Spawned visible acid core and collision have the intended doubled world dimensions, without inherited-scale multiplication"));
 Flight->MuzzleEffect=nullptr;Flight->BeginPlay();Projectile->SetActorLocation(FVector(3599,0,5000));Flight->TickComponent(.1f,LEVELTICK_All,nullptr);Expect(!Projectile->IsActorBeingDestroyed(),TEXT("Projectile remains alive near the extended range"));
 Projectile->SetActorLocation(FVector(3601,0,5000));Flight->TickComponent(.1f,LEVELTICK_All,nullptr);Expect(Projectile->IsActorBeingDestroyed(),TEXT("Projectile is still cleaned up at the extended range limit"));
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
