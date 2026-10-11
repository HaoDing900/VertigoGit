#include "NarrativeComponent.h"
#include "NarrativeSequenceMontageEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"

bool VTGTestSequenceMontages()
{
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SequenceMontageTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
 auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Characters/Sa/Anm/Terminal/ANM_Sa_LoopWatchingTerminal1_Montage"));
 if(!BP || !Montage)return false;
 auto* Actor=W->SpawnActor<ACharacter>();
 auto* Mesh=Actor->GetMesh();
 Mesh->SetSkeletalMesh(CastChecked<ACharacter>(BP->GeneratedClass->GetDefaultObject())->GetMesh()->GetSkeletalMeshAsset());
 Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass()); Mesh->InitAnim(true);
 auto* Anim=Mesh->GetAnimInstance();
 auto* Comp=NewObject<UNarrativeComponent>(Actor); Actor->AddInstanceComponent(Comp); Comp->RegisterComponent();
 auto* Sequence=NewObject<ULevelSequence>(); Sequence->Initialize();
 Sequence->GetMovieScene()->SetPlaybackRange(0,24000);
 auto Guid=Sequence->GetMovieScene()->AddPossessable(TEXT("Character"),ACharacter::StaticClass());
 Sequence->BindPossessableObject(Guid,*Actor,W);
 int32 Fail=0;
 auto Expect=[&](bool OK,const TCHAR* Message){ if(!OK)++Fail; UE_LOG(LogTemp,Display,TEXT("SEQUENCE MONTAGE %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),Message); };
 auto Play=[&](){Expect(Anim && Anim->Montage_Play(Montage,1.25f)>0,TEXT("real authored Montage plays")); Anim->Montage_SetPosition(Montage,.25f);};
 Play();
 auto* Player=Comp->PlayDialogueLevelSequence(Sequence,false,false,1.f,0,true,false,0.f,false);
 Expect(Player && Anim->Montage_IsPlaying(Montage) && !Actor->FindComponentByClass<UNarrativeSequenceMontageGuard>(),TEXT("default off leaves Montage untouched"));
 Comp->StopDialogueLevelSequence();
 Play();
 Player=Comp->PlayDialogueLevelSequence(Sequence,false,false,1.f,0,true,false,0.f,true);
 auto* Guard=Actor->FindComponentByClass<UNarrativeSequenceMontageGuard>();
 Expect(Player && Guard && !Anim->Montage_IsActive(Montage),TEXT("enabled stops bound actor Montage before Play"));
 Player->Pause();
 Guard->TickComponent(0.f,LEVELTICK_All,nullptr);
 Expect(!Anim->Montage_IsActive(Montage),TEXT("sequence pause keeps Montage suspended"));
 Comp->StopDialogueLevelSequence();
 Guard->TickComponent(0.f,LEVELTICK_All,nullptr);
 Expect(Anim->Montage_IsPlaying(Montage) && FMath::IsNearlyEqual(Anim->Montage_GetPosition(Montage),.25f) && FMath::IsNearlyEqual(Anim->Montage_GetPlayRate(Montage),1.25f),TEXT("manual Stop restores position and speed"));
 Anim->Montage_Pause(Montage);
 Player=Comp->PlayDialogueLevelSequence(Sequence,false,false,1.f,0,true,false,0.f,true);
 Guard=Actor->FindComponentByClass<UNarrativeSequenceMontageGuard>();
 Player->Update(2.f);
 Expect(!Player->IsPlaying() && !Player->IsPaused(),TEXT("sequence naturally reaches Finished"));
 Guard->TickComponent(0.f,LEVELTICK_All,nullptr);
 Expect(Anim->Montage_IsActive(Montage) && !Anim->Montage_IsPlaying(Montage),TEXT("paused Montage stays paused on restore"));
 Comp->StopDialogueLevelSequence();
 W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
 return Fail==0;
}


