#include "Combat/VTGFistWindComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "UObject/UnrealType.h"

bool VTGVerifyEnhancedWind()
{
 auto Expect=[](bool OK,const TCHAR* Text){checkf(OK,TEXT("%s"),Text);UE_LOG(LogTemp,Display,TEXT("ENHANCED_WIND_TEST PASS %s"),Text);};
 auto* Fist=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_fist_Wind"));auto* Hammer=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_hammer_Wind"));auto* Original=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast"));check(Fist && Hammer && Original);
 auto Color=[](UNiagaraSystem* S,FName Name){return S->GetExposedParameters().GetParameterValue<FLinearColor>(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(),Name));};
 Expect(FMath::IsNearlyEqual(Color(Fist,TEXT("User.Color Minimum")).A,.18f) && FMath::IsNearlyEqual(Color(Fist,TEXT("User.Color Maximum")).A,.45f),TEXT("Fist opacity raised to 0.18-0.45"));
 Expect(FMath::IsNearlyEqual(Color(Original,TEXT("User.Color Maximum")).A,.13f),TEXT("Original environmental wind asset is unchanged"));
 for(auto* S:{Fist,Hammer})
 {
  auto& P=S->GetExposedParameters();auto Float=[&](FName Name){return P.GetParameterValue<float>(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),Name));};
  Expect(Float(TEXT("User.Lifetime Max"))<=.18f && Float(TEXT("User.Spawn Rate Curve Mult"))==30.f && S->GetEmitterHandles().Num()==1 && S->GetEmitterHandles()[0].GetEmitterData()->bLocalSpace && !S->NeedsWarmup(),TEXT("Stronger visibility preserves short lifetime, spawn budget, local space and single emitter"));
 }
 TGuardValue<bool> Script(GAllowActorScriptExecutionInEditor,true);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("EnhancedCombatWindTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Sa=W->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn);auto* C=Sa->FindComponentByClass<UVTGFistWindComponent>();check(C);
 auto Flag=[&](FName Name,bool Value){auto* P=FindFProperty<FBoolProperty>(Sa->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Sa,Value);};
 auto* Weapon=FindFProperty<FByteProperty>(Sa->GetClass(),TEXT("CurrentMeleeWeapon"));check(Weapon);
 Flag(TEXT("IsDead?"),false);Flag(TEXT("HidePlayerNCam"),false);Flag(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);Flag(TEXT("IsAttacking"),true);Flag(TEXT("HasWeaponInHand"),true);
 FindFProperty<FIntProperty>(Sa->GetClass(),TEXT("Health"))->SetPropertyValue_InContainer(Sa,100);
 Sa->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());Sa->GetMesh()->InitAnim(true);auto* Anim=Sa->GetMesh()->GetAnimInstance();check(Anim);
 UStaticMeshComponent* Mesh=nullptr;for(auto* Part:Sa->GetComponents())if(Part->GetFName()==TEXT("MeleeWeapon"))Mesh=Cast<UStaticMeshComponent>(Part);check(Mesh);
 Mesh->SetStaticMesh(C->HammerMesh);Mesh->SetVisibility(true);Mesh->SetHiddenInGame(false);Weapon->SetPropertyValue_InContainer(Sa,1);
 Expect(C->HammerWindSystem==Hammer && C->HammerMontages.Num()==3 && C->WindScale==1.25f,TEXT("Saved player has three actual hammer attacks and enlarged fist wind"));
 Flag(TEXT("InCombat"),false);Anim->Montage_Play(C->HammerMontages[0]);Expect(!C->WantsHammerWind(),TEXT("Hammer attacks outside combat cannot spawn wind"));Flag(TEXT("InCombat"),true);
 UAnimMontage* Last=nullptr;float LastEnd=0;
 for(UAnimMontage* M:C->HammerMontages)
 {
  Anim->Montage_Play(M);const FAnimNotifyEvent* Hit=nullptr;for(const auto& E:M->Notifies)if(E.NotifyStateClass && E.NotifyStateClass->GetClass()->GetName().Contains(TEXT("Notify_Damage"))){if(!Hit)Hit=&E;LastEnd=FMath::Max(Last==M?LastEnd:0.f,E.GetTime()+E.GetDuration());Last=M;}
  check(Hit);Anim->Montage_SetPosition(M,Hit->GetTime()+Hit->GetDuration()*.5f);Expect(C->WantsHammerWind(),TEXT("Actual hammer attack hit window enables wind"));
 }
 C->TickComponent(.016f,LEVELTICK_All,nullptr);Expect(IsValid(C->HammerWind) && C->Winds.IsEmpty(),TEXT("Hammer swing creates exactly one head effect and no fist effects"));
 auto* Wind=C->HammerWind.Get();Expect(Wind->GetAttachParent()==Mesh && Wind->GetComponentLocation().Equals(Mesh->GetComponentTransform().TransformPosition(C->HammerWindOffset),.01),TEXT("Hammer wind is attached at the configured hammer-head model position"));
 Mesh->AddLocalRotation(FRotator(20,35,-12));Expect(Wind->GetComponentLocation().Equals(Mesh->GetComponentTransform().TransformPosition(C->HammerWindOffset),.01) && FVector::DotProduct(Wind->GetForwardVector(),Mesh->GetComponentQuat().RotateVector(C->HammerWindRotation.Vector()))>.999f,TEXT("Wind position and visual direction follow the moving hammer"));
 for(int I=0;I<20;++I)C->TickComponent(.016f,LEVELTICK_All,nullptr);Expect(C->HammerWind.Get()==Wind,TEXT("Per-frame updates reuse the active effect instead of stacking components"));
 Anim->Montage_SetPosition(Last,LastEnd+.12f);Expect(!C->WantsHammerWind(),TEXT("Long hammer recovery after the final hit window has no wind"));C->TickComponent(.016f,LEVELTICK_All,nullptr);Expect(!C->HammerWind,TEXT("End of swing destroys hammer Niagara immediately"));
 Anim->Montage_Play(C->HammerMontages[0]);Anim->Montage_SetPosition(C->HammerMontages[0],.8f);Anim->Montage_Stop(0);Expect(!C->WantsHammerWind(),TEXT("Interrupted montage cannot keep wind alive with a stale attack flag"));
 Flag(TEXT("HasWeaponInHand"),false);Weapon->SetPropertyValue_InContainer(Sa,0);Anim->Montage_Play(C->PunchMontages[0]);C->TickComponent(.016f,LEVELTICK_All,nullptr);Expect(C->Winds.Num()==2 && !C->HammerWind,TEXT("Switching back to fists restores only the two fist effects"));
 Flag(TEXT("InCombat"),false);C->TickComponent(.016f,LEVELTICK_All,nullptr);TInlineComponentArray<UNiagaraComponent*> Components(Sa);
 Expect(C->Winds.IsEmpty() && !C->HammerWind && !Components.ContainsByPredicate([&](auto* N){return N->GetAsset()==Fist || N->GetAsset()==Hammer;}),TEXT("Combat exit fully unregisters both wind types; no idle Niagara simulation remains"));
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
