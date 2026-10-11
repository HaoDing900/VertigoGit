#include "NarrativeSequenceMontageEvent.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UNarrativeSequenceMontageGuard::UNarrativeSequenceMontageGuard()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}
void UNarrativeSequenceMontageGuard::Begin(ULevelSequencePlayer* Player)
{
 if (Players.Contains(Player)) return;
 if (Players.IsEmpty())
 {
  TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
  for (auto* Mesh : Meshes)
   if (auto* Anim = Mesh->GetAnimInstance())
   {
    for (auto* Instance : Anim->MontageInstances)
     if (Instance && Instance->IsActive() && Instance->Montage)
     {
      auto& State = Saved.AddDefaulted_GetRef();
      State.Mesh = Mesh; State.Anim = Anim; State.Montage = Instance->Montage;
      State.Position = Instance->GetPosition(); State.Rate = Instance->GetPlayRate(); State.bPlaying = Instance->IsPlaying();
      for(int32 I=0; I<State.Montage->CompositeSections.Num(); ++I) State.NextSections.Add(Instance->GetNextSectionID(I));
     }
    Anim->Montage_Stop(0.f);
   }
 }
 Players.Add(Player);
}
void UNarrativeSequenceMontageGuard::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Delta,TickType,Tick);
 // Paused sequences retain ownership; stopped/finished players release it after Sequencer restores animation state.
 Players.RemoveAll([](const TWeakObjectPtr<ULevelSequencePlayer>& P){return !P.IsValid() || (!P->IsPlaying() && !P->IsPaused());});
 if(Players.IsEmpty()) { Restore(); DestroyComponent(); return; }
 TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
 for(auto* Mesh:Meshes) if(auto* Anim=Mesh->GetAnimInstance()) Anim->Montage_Stop(0.f);
}
void UNarrativeSequenceMontageGuard::Restore()
{
 auto States = MoveTemp(Saved);
 for(const auto& State:States)
 {
  auto* Mesh=State.Mesh.Get(); auto* Anim=State.Anim.Get();
  if(!Mesh || !Anim || Mesh->GetAnimInstance()!=Anim || !State.Montage) continue;
  if(Anim->Montage_Play(State.Montage,State.Rate,EMontagePlayReturnType::MontageLength,State.Position,false)<=0.f) continue;
  for(int32 I=0; I<State.NextSections.Num(); ++I)
  {
   const int32 Next=State.NextSections[I];
   Anim->Montage_SetNextSection(State.Montage->GetSectionName(I),Next==INDEX_NONE?NAME_None:State.Montage->GetSectionName(Next),State.Montage);
  }
  if(!State.bPlaying) Anim->Montage_Pause(State.Montage);
 }
}
void UNarrativeSequenceMontageGuard::EndPlay(const EEndPlayReason::Type Reason)
{
 Saved.Reset(); Players.Reset(); Super::EndPlay(Reason);
}
void UNarrativeSequenceMontageGuard::Prepare(ULevelSequencePlayer* Player)
{
 if(!Player || !Player->GetSequence()) return;
 auto* MovieScene=Player->GetSequence()->GetMovieScene();
 if(!MovieScene) return;
 TSet<AActor*> Actors;
 auto GatherActors = [&](UMovieSceneSequence* Sequence, FMovieSceneSequenceID SequenceID)
 {
  if(!Sequence || !Sequence->GetMovieScene()) return;
  for(const auto& Binding:Sequence->GetMovieScene()->GetBindings())
   for(auto* Bound:Player->GetBoundObjects(FMovieSceneObjectBindingID(UE::MovieScene::FFixedObjectBindingID(Binding.GetObjectGuid(), SequenceID))))
    if(Bound)
    {
     if(auto* Actor=Cast<AActor>(Bound)) Actors.Add(Actor);
     else if(auto* Component=Cast<UActorComponent>(Bound)) Actors.Add(Component->GetOwner());
    }
 };
 GatherActors(Player->GetSequence(),MovieSceneSequenceID::Root);
 if(const auto* Hierarchy=Player->GetEvaluationTemplate().GetHierarchy())
  for(const auto& Pair:Hierarchy->AllSubSequenceData()) GatherActors(Pair.Value.GetSequence(),Pair.Key);
 for(auto* Actor:Actors)
 {
  if(!IsValid(Actor))continue;
  auto* Guard=Actor->FindComponentByClass<UNarrativeSequenceMontageGuard>();
  if(!Guard) { Guard=NewObject<UNarrativeSequenceMontageGuard>(Actor); Actor->AddInstanceComponent(Guard); Guard->RegisterComponent(); }
  Guard->Begin(Player);
 }
}




