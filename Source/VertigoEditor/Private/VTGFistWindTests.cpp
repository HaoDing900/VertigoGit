#include "Combat/VTGFistWindComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

bool VTGVerifyFistWind()
{
    auto Expect=[](bool OK,const TCHAR* Text){checkf(OK,TEXT("%s"),Text);UE_LOG(LogTemp,Display,TEXT("FIST_TEST PASS %s"),Text);};
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
    auto* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_fist_Wind"));check(System);
    auto* Original=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast"));check(Original);
    auto Float=[](UNiagaraSystem* S,const TCHAR* Name){return S->GetExposedParameters().GetParameterValue<float>(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),Name));};
    Expect(Float(System,TEXT("User.Lifetime Max"))<=.18f && Float(Original,TEXT("User.Lifetime Max"))==5.f,TEXT("New wind has short lifetime; original is unchanged"));
    Expect(Float(System,TEXT("User.Spawn Rate Curve Mult"))==30.f && Float(Original,TEXT("User.Spawn Rate Curve Mult"))==3.f,TEXT("Spawn rate increased only on duplicate"));
    Expect(System->GetEmitterHandles().Num()==1 && System->GetEmitterHandles()[0].GetEmitterData()->bLocalSpace && !System->NeedsWarmup(),TEXT("Single local-space emitter, no warmup"));
    TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor,true);
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("FistWindTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Sa=W->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn);check(Sa);
    auto* C=Sa->FindComponentByClass<UVTGFistWindComponent>();check(C);
    auto Flag=[&](FName Name,bool Value){auto* P=FindFProperty<FBoolProperty>(Sa->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Sa,Value);};
    auto Int=[&](FName Name,int32 Value){auto* P=FindFProperty<FIntProperty>(Sa->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Sa,Value);};
    auto Health=[&](){return FindFProperty<FIntProperty>(Sa->GetClass(),TEXT("Health"))->GetPropertyValue_InContainer(Sa);};
    Flag(TEXT("IsDead?"),false);Flag(TEXT("InCombat"),false);Flag(TEXT("HidePlayerNCam"),false);Flag(TEXT("HasWeaponInHand"),false);Flag(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);Int(TEXT("Health"),100);Int(TEXT("MaxHealth_Normal"),100);
    Sa->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());Sa->GetMesh()->InitAnim(true);auto* Anim=Sa->GetMesh()->GetAnimInstance();check(Anim);
    Expect(C->WindSystem==System && C->PunchMontages.Num()==3 && C->RecoveryDelay==30.f,TEXT("Player component references exact effect, unarmed combo set, and 30-second delay"));
    Flag(TEXT("IsAttacking"),true);Anim->Montage_Play(C->PunchMontages[0]);
    Expect(!C->WantsWind(),TEXT("Attack flag and montage cannot create wind outside combat"));
    Flag(TEXT("InCombat"),true);C->UpdateCombatState(true);
    for(UAnimMontage* Montage:C->PunchMontages)
    {
        Anim->Montage_Play(Montage);
        Expect(C->WantsWind(),TEXT("Authored unarmed combo montage enables fist wind"));
    }
    C->SetWindActive(C->WantsWind());Expect(C->Winds.Num()==2,TEXT("Two wind components created on both brass knuckles"));
    for(UNiagaraComponent* Wind:C->Winds)Expect(Wind->GetAttachParent() && Wind->GetAttachParent()->GetName().Contains(TEXT("BrassKnuckle")),TEXT("Wind is attached to actual knuckle transform"));
    for(UNiagaraComponent* Wind:C->Winds)
    {
        const bool Left=Wind->GetAttachParent()->GetAttachSocketName().ToString().ToLower().Contains(TEXT("_l")) || Wind->GetAttachParent()->GetName().ToLower().Contains(TEXT("left"));
        const FVector Direction=(Sa->GetMesh()->GetSocketLocation(Left?TEXT("c_middle1_l"):TEXT("c_middle1_r"))-Sa->GetMesh()->GetSocketLocation(Left?TEXT("hand_l"):TEXT("hand_r"))).GetSafeNormal();
        Expect(!Direction.IsNearlyZero() && FVector::DotProduct(Wind->GetForwardVector(),Direction)>.999f,TEXT("Wind visual X axis matches actual wrist-to-knuckle direction on each hand"));
    }
    C->SetWindActive(true);Expect(C->Winds.Num()==2,TEXT("Repeated updates never stack extra wind components"));
    Flag(TEXT("HasWeaponInHand"),true);Expect(!C->WantsWind(),TEXT("Weapon equipment suppresses fist effect"));Flag(TEXT("HasWeaponInHand"),false);
    Flag(TEXT("IsAttacking"),false);C->SetWindActive(C->WantsWind());Expect(C->Winds.IsEmpty(),TEXT("Stopping attack immediately destroys wind components"));
    Flag(TEXT("IsAttacking"),true);Anim->Montage_Stop(0.f);Expect(!C->WantsWind(),TEXT("Interrupted montage cannot leave wind on with stale attack flag"));
    Anim->Montage_Play(C->PunchMontages[0]);C->SetWindActive(true);
    Flag(TEXT("InCombat"),false);C->TickComponent(.016f,LEVELTICK_All,nullptr);
    TInlineComponentArray<UNiagaraComponent*> Remaining(Sa);
    Expect(C->Winds.IsEmpty() && !Remaining.ContainsByPredicate([&](UNiagaraComponent* N){return N->GetAsset()==System;}),TEXT("Leaving combat unregisters/destroys Niagara; no dormant wind simulation remains"));
    auto Advance=[&](float Seconds){++GFrameCounter;W->GetTimerManager().Tick(Seconds);};
    Int(TEXT("Health"),20);Flag(TEXT("ShowLowHealth?"),true);
    Advance(0.f);Advance(29.f);Expect(Health()==20,TEXT("No early healing before 30 seconds"));
    Advance(1.01f);Expect(Health()==100 && !FindFProperty<FBoolProperty>(Sa->GetClass(),TEXT("ShowLowHealth?"))->GetPropertyValue_InContainer(Sa),TEXT("30 seconds out of combat uses original healing event and clears injured state"));
    Flag(TEXT("InCombat"),true);C->UpdateCombatState(true);Int(TEXT("Health"),25);
    Flag(TEXT("InCombat"),false);C->UpdateCombatState(false);Advance(0.f);Advance(15.f);
    Flag(TEXT("InCombat"),true);C->UpdateCombatState(true);Advance(31.f);Expect(Health()==25,TEXT("Combat re-entry cancels pending full heal"));
    Flag(TEXT("InCombat"),false);C->UpdateCombatState(false);Advance(0.f);Advance(29.f);Expect(Health()==25,TEXT("Next combat exit starts a fresh full 30-second countdown"));
    Advance(1.01f);Expect(Health()==100,TEXT("Fresh countdown heals normally"));
    Flag(TEXT("InCombat"),true);C->UpdateCombatState(true);Flag(TEXT("InCombat"),false);C->UpdateCombatState(false);Int(TEXT("Health"),0);Flag(TEXT("IsDead?"),true);Advance(0.f);Advance(31.f);
    Expect(Health()==0,TEXT("Recovery never revives a dead player"));
    Flag(TEXT("IsDead?"),false);Int(TEXT("Health"),15);Int(TEXT("MaxHealth_Normal"),80);C->RecoverHealth();Expect(Health()==80,TEXT("Recovery uses authored max health instead of hardcoded 100"));
    C->SetWindActive(false);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return true;
}



