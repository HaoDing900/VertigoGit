#include "VTGNoteInputTestFixture.h"
#include "UI/VTGSequenceDialogue.h"
#include "NarrativeUIInputComponent.h"
#include "NarrativeComponent.h"
#include "NarrativeEvent.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "UObject/UnrealType.h"

bool VTGVerifyCorvosSequenceRegression()
{
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 auto Expect=[](bool OK,const TCHAR* Label){checkf(OK,TEXT("%s"),Label);UE_LOG(LogTemp,Display,TEXT("CORVOS_REGRESSION PASS %s"),Label);};
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CorvosSequenceRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* SaClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));auto* CorvosClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/Characters/NPC/Corvos/BP_Corvos_NPC.BP_Corvos_NPC_C"));check(SaClass && CorvosClass);
 auto* Sa=W->SpawnActor<ACharacter>(SaClass,FTransform::Identity,Spawn);auto* Corvos=W->SpawnActor<ACharacter>(CorvosClass,FTransform::Identity,Spawn);check(Sa && Corvos);
 auto* PC=W->SpawnActor<AVTGPlayerController>();PC->Possess(Sa);W->AddController(PC);
 auto* N=Sa->FindComponentByClass<UNarrativeComponent>();check(N);auto* Current=FindFProperty<FObjectPropertyBase>(N->GetClass(),TEXT("CurrentDialogue"));check(Current);
 auto Begin=[&](){auto* D=NewObject<UVTGNoteInputTestDialogue>(N);D->OwningComp=N;D->bCanBeExited=false;Current->SetObjectPropertyValue_InContainer(N,D);D->Start(PC);return D;};
 auto* BP=LoadObject<UDialogueBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still"));check(BP);
 auto* Template=CastChecked<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass)->GetDialogueTemplate();UDialogueNode* Node=nullptr;
 for(auto* Item:Template->GetNodes())if(Item->GetID()==TEXT("L3PoliceStation_Still_DialogueNode_NPC_6"))Node=Item;check(Node);
 UNarrativeEvent* EventTemplate=nullptr;for(auto* E:Node->Events)if(E && E->GetClass()->GetName()==TEXT("1PlayLevelSequence_C"))EventTemplate=E;check(EventTemplate);
 auto* Sequence=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_CorvosConv_SaApproach"));check(Sequence);
 auto* SequenceProperty=FindFProperty<FObjectPropertyBase>(EventTemplate->GetClass(),TEXT("Level Sequence"));check(SequenceProperty);
 Expect(SequenceProperty->GetObjectPropertyValue_InContainer(EventTemplate)==Sequence,TEXT("Actual Corvos NPC_6 uses 1PlayLevelSequence with the reported SaApproach asset"));
 // Bind this process's actors in memory only. No map, Sequence or dialogue asset is saved.
 for(int32 I=0;I<Sequence->GetMovieScene()->GetPossessableCount();++I)
 {
  auto& B=Sequence->GetMovieScene()->GetPossessable(I);
  if(B.GetParent().IsValid())continue;
  AActor* Actor=B.GetPossessedObjectClass()==SaClass?Sa:B.GetPossessedObjectClass()==CorvosClass?Corvos:nullptr;
  if(Actor){Sequence->UnbindPossessableObjects(B.GetGuid());Sequence->BindPossessableObject(B.GetGuid(),*Actor,W);}
 }
 auto* D=Begin();Expect(PC->IsMoveInputIgnored() && PC->IsLookInputIgnored(),TEXT("Dialogue owns player movement/look input before its event starts"));
 auto* Event=DuplicateObject<UNarrativeEvent>(EventTemplate,D);Event->ExecuteEvent(Sa,PC,N);
 auto* Player=N->GetDialogueLevelSequencePlayer();auto* Actor=Player?Cast<ALevelSequenceActor>(Player->GetOuter()):nullptr;
 Expect(Player && Actor && Player->IsPlaying() && N->GetCurrentDialogue()==D,TEXT("Actual Narrative event starts Corvos Sequence without exiting its owning dialogue"));
 Expect(Actor->ActorHasTag(TEXT("Narrative.DialogueShot")) && !UVTGSequenceDialogueGate::ShouldEndDialogue(Actor),TEXT("Narrative event player is protected before the entry hook can fire"));
 Actor->Tags.Add(UVTGSequenceDialogueGate::EndDialogueTag);
 Expect(!UVTGSequenceDialogueGate::ShouldEndDialogue(Actor),TEXT("Explicit exit opt-in cannot override Narrative sequence protection"));
 Actor->Tags.Remove(TEXT("Narrative.DialogueShot"));
 Expect(!UVTGSequenceDialogueGate::ShouldEndDialogue(Actor),TEXT("Authoritative Narrative player ownership protects even a missing tag"));
 for(int I=0;I<25;++I)Player->Update(1.f/30.f);
 Expect(Player->IsPlaying() && Player->GetCurrentTime().AsSeconds()>.7 && N->GetCurrentDialogue()==D,TEXT("Corvos Sequence is still playing partway through instead of being torn down"));
 Expect(PC->IsMoveInputIgnored() && PC->IsLookInputIgnored(),TEXT("Player control remains locked during the approach animation"));
 N->StopDialogueLevelSequence();Expect(N->GetCurrentDialogue()==D && PC->IsMoveInputIgnored(),TEXT("Stopping only the owned Sequence does not end its parent dialogue or release its input"));
 auto* Direct=N->PlayDialogueLevelSequence(Sequence,false,false,1,0,true,false,0,false);check(Direct);
 Expect(N->GetCurrentDialogue()==D && Direct->IsPlaying(),TEXT("Direct Narrative-component playback uses the same protected ownership path"));
 const double Duration=Direct->GetDuration().AsSeconds();
 Direct->Update(float(Duration+.1));
 Expect(!Direct->IsPlaying() && N->GetCurrentDialogue()==D,TEXT("Full Corvos Sequence can finish naturally without prematurely terminating its dialogue"));
 N->ForceExitDialogue();Expect(!N->GetCurrentDialogue() && !N->GetDialogueLevelSequencePlayer(),TEXT("Explicit Force Exit still cleans up its dialogue-owned Sequence"));
 // External cutscenes default OFF and require explicit opt-in.
 D=Begin();auto* External=NewObject<ULevelSequence>(W);External->Initialize();External->GetMovieScene()->SetPlaybackRange(0,240000);
 ALevelSequenceActor* ExternalActor=nullptr;auto* ExternalPlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(W,External,FMovieSceneSequencePlaybackSettings(),ExternalActor);check(ExternalPlayer);
 Expect(!UVTGSequenceDialogueGate::ShouldEndDialogue(ExternalActor),TEXT("External cutscene defaults OFF"));ExternalPlayer->Play();
 Expect(N->GetCurrentDialogue()==D,TEXT("Default-OFF external cutscene preserves dialogue"));ExternalPlayer->Stop();
 ExternalActor->Tags.Add(UVTGSequenceDialogueGate::EndDialogueTag);ExternalPlayer->Play();
 Expect(!N->GetCurrentDialogue(),TEXT("External cutscene still force exits an old non-exitable dialogue"));ExternalPlayer->Stop();
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
