#include "VTGNPCSequenceIdleTargets.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Slot.h"
#include "AnimSequencerInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Editor.h"

bool VTGTestNPCSequenceIdle(bool RequireFixed)
{
 FEditorScriptExecutionGuard Guard;int Failures=0;
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("NPCSequenceIdleTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& WorldContext=GEngine->CreateNewWorldContext(EWorldType::Game);WorldContext.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);WorldContext.OwningGameInstance=GI;
 for(const auto& T:VTGNPCSequenceIdleTargets())
 {
  auto* BP=LoadObject<UAnimBlueprint>(nullptr,T.Blueprint);auto* ActorBP=LoadObject<UBlueprint>(nullptr,T.Actor);auto* Idle=LoadObject<UAnimSequence>(nullptr,T.Fallback);check(BP && ActorBP && Idle);
  FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* A=W->SpawnActor<ACharacter>(ActorBP->GeneratedClass,FTransform::Identity,Spawn);check(A);auto* M=A->GetMesh();check(M && M->GetSkeletalMeshAsset());
  check(BP->TargetSkeleton->IsCompatibleForEditor(M->GetSkeletalMeshAsset()->GetSkeleton()));M->SetAnimInstanceClass(BP->GeneratedClass);M->InitAnim(true);UAnimInstance* Anim=M->GetAnimInstance();check(Anim);
  auto Expect=[&](bool OK,const TCHAR* Message){if(!OK && RequireFixed)++Failures;UE_LOG(LogTemp,Display,TEXT("NPC IDLE %s %s: %s"),*BP->GetName(),OK?TEXT("PASS"):RequireFixed?TEXT("FAIL"):TEXT("BEFORE"),Message);};
  auto SetIdle=[&](UAnimSequence* Clip){auto* ActorIdle=FindFProperty<FObjectPropertyBase>(A->GetClass(),TEXT("IdleAnim"));check(ActorIdle);ActorIdle->SetObjectPropertyValue_InContainer(A,Clip);auto* F=Anim->FindFunctionChecked(TEXT("SetBaseIdle"));FStructOnScope Args(F);FindFProperty<FObjectPropertyBase>(F,TEXT("NewIdle"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Clip);Anim->ProcessEvent(F,Args.GetStructMemory());};
  auto Cut=[&](bool Value){auto* P=FindFProperty<FBoolProperty>(Anim->GetClass(),TEXT("Is Cutscene"));check(P);P->SetPropertyValue_InContainer(Anim,Value);};
  auto Frame=[&](float Dt=1.f/60.f){++GFrameCounter;M->TickAnimation(Dt,false);M->RefreshBoneTransforms();};
  auto Delta=[&](){float D=0;int N=0;const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();const auto& Pose=M->GetBoneSpaceTransforms();for(int I=0;I<Ref.GetNum();++I){const auto Name=Ref.GetBoneName(I).ToString();if(Name.Contains(TEXT("arm")) || Name.Contains(TEXT("hand")) || Name.Contains(TEXT("spine")) || Name.Contains(TEXT("thigh")) || Name.Contains(TEXT("calf")) || Name.Contains(TEXT("leg")) || Name.Contains(TEXT("hips"))){D=FMath::Max(D,Pose[I].GetRotation().AngularDistance(Ref.GetRefBonePose()[I].GetRotation()));++N;}}check(N>5);return D;};
  auto CheckPose=[&](const TCHAR* Label){const float D=Delta();UE_LOG(LogTemp,Display,TEXT("NPC IDLE POSE %s %s max-body-angle=%.6f"),*BP->GetName(),Label,D);Expect(D>.05f,Label);};
  Frame();CheckPose(TEXT("initial/default idle"));
  SetIdle(Idle);Cut(false);Frame();CheckPose(TEXT("authored idle"));SetIdle(nullptr);Frame();CheckPose(TEXT("null idle request"));
  auto* IdleProp=FindFProperty<FObjectPropertyBase>(Anim->GetClass(),TEXT("Base Idle Anm"));check(IdleProp);Expect(IdleProp->GetObjectPropertyValue_InContainer(Anim)==Idle,TEXT("null request preserves the last valid idle"));
  auto* Alternate=DuplicateObject<UAnimSequence>(Idle,GetTransientPackage());SetIdle(Alternate);Frame();Expect(IdleProp->GetObjectPropertyValue_InContainer(Anim)==Alternate,TEXT("valid runtime idle replacement remains supported"));
  Cut(true);for(int I=0;I<15;++I)Frame();CheckPose(TEXT("cinematic slot without animation"));

  FName SlotName=NAME_None;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(G->GetName()==TEXT("AnimGraph") && N->GetFName()==T.CinematicNode)SlotName=CastChecked<UAnimGraphNode_Slot>(N)->Node.SlotName;check(!SlotName.IsNone());
  auto* Seq=NewObject<ULevelSequence>(GetTransientPackage());Seq->Initialize();auto* Scene=Seq->GetMovieScene();Scene->SetTickResolutionDirectly(FFrameRate(24000,1));Scene->SetPlaybackRange(0,6000);
  auto GUID=Scene->AddPossessable(TEXT("NPC"),ACharacter::StaticClass());Seq->BindPossessableObject(GUID,*A,W);
  auto* Track=Scene->AddTrack<UMovieSceneSkeletalAnimationTrack>(GUID);check(Track);auto* Section=CastChecked<UMovieSceneSkeletalAnimationSection>(Track->AddNewAnimation(FFrameNumber(0),Idle));Section->Params.SlotName=SlotName;Section->SetRange(TRange<FFrameNumber>(0,6000));Section->SetCompletionMode(EMovieSceneCompletionMode::RestoreState);
  FMovieSceneSequencePlaybackSettings Settings;Settings.bRestoreState=true;ALevelSequenceActor* SequenceActor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,Settings,SequenceActor);check(Player && SequenceActor);
  for(bool NaturalFinish:{false,true})
  {
   Cut(true);Frame();Player->Play();Player->Update(.1f);Frame();
   auto* During=M->GetAnimInstance();const bool Evaluated=During && (During->GetCurrentActiveMontage()!=nullptr || During->IsA<UAnimSequencerInstance>());Expect(Evaluated,TEXT("real skeletal animation Sequence takes over the body"));
   if(NaturalFinish)Player->Update(.5f);else Player->Stop();Expect(!Player->IsPlaying(),TEXT("Sequence stops/finishes"));
   Anim=M->GetAnimInstance();check(Anim && Anim->IsA(BP->GeneratedClass));Frame(0.f);CheckPose(NaturalFinish?TEXT("natural finish same frame"):TEXT("manual stop same frame"));
   Cut(false);Frame(0.f);CheckPose(TEXT("clear cutscene flag same frame"));
   float Minimum=Delta();for(int I=0;I<12;++I){Frame();Minimum=FMath::Min(Minimum,Delta());}Expect(Minimum>.05f,TEXT("all 13 exit frames retain a non-reference body pose"));
   auto* P=FindFProperty<FObjectPropertyBase>(Anim->GetClass(),TEXT("Base Idle Anm"));Expect(P && P->GetObjectPropertyValue_InContainer(Anim)==Alternate,TEXT("Sequence teardown preserves the requested idle"));
  }
  if(BP->GetName()==TEXT("ABP_NPC_May"))
  {
   // Exercise the actual yard handoff, including its synchronous pose refresh.
   auto* ManagerBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/BPLM_ShoppingCenter_ClinicYard"));check(ManagerBP);
   auto* Manager=W->SpawnActor<AActor>(ManagerBP->GeneratedClass,FTransform::Identity,Spawn);check(Manager);
   TArray<AActor*> Helpers;
   for(const FName Name:{FName(TEXT("MayCine")),FName(TEXT("Player Cinematic")),FName(TEXT("Drone"))})
   {
    auto* P=FindFProperty<FObjectPropertyBase>(Manager->GetClass(),Name);check(P);
    AActor* Value=A;
    if(Name!=TEXT("MayCine")){Value=W->SpawnActor<AActor>(P->PropertyClass,FTransform::Identity,Spawn);check(Value);Helpers.Add(Value);}
    P->SetObjectPropertyValue_InContainer(Manager,Value);
   }
   SetIdle(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/ANM/ANM_LS_L5_MayPlayingKnife")));Cut(true);Frame();
   Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("L5_Stairs_PositionNIdle")),nullptr);
   Expect(IdleProp->GetObjectPropertyValue_InContainer(Anim)==Idle,TEXT("actual L5_Stairs_PositionNIdle assigns May's authored stair idle"));
   CheckPose(TEXT("actual yard handoff immediate pose refresh"));
   for(int I=0;I<13;++I){Frame();Expect(Delta()>.05f,TEXT("actual yard handoff frame retains body pose"));}
   for(auto* Helper:Helpers)Helper->Destroy();Manager->Destroy();
  }
  SequenceActor->Destroy();A->Destroy();
  UE_LOG(LogTemp,Display,TEXT("NPC IDLE TEST COMPLETE %s cumulative-failures=%d"),*BP->GetName(),Failures);
 }
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return Failures==0;
}
