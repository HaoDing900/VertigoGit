#include "Audio/VTGCutsceneAudio.h"
#include "Components/AudioComponent.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundCue.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "UObject/UnrealType.h"

bool VTGVerifyCutsceneAudio()
{
    auto Expect=[](bool OK,const TCHAR* Text){checkf(OK,TEXT("%s"),Text);UE_LOG(LogTemp,Display,TEXT("CUTSCENE_TEST PASS %s"),Text);};
    auto* ManagerBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager"));
    auto* LM=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM"));check(ManagerBP && LM);
    for(auto* BP:{ManagerBP,LM})for(FName Name:{TEXT("Audio_PlayPsych"),TEXT("Audio_StopPsych"),TEXT("Audio_PlayOneShot"),TEXT("Audio_StopOneShot")})
        Expect(!BP->GeneratedClass->FindFunctionByName(Name),TEXT("Retired shared-channel function removed"));
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CutsceneAudioTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* State=W->GetSubsystem<UVTGCutsceneAudioSubsystem>();check(State);
    auto* Manager=W->SpawnActor<AActor>(ManagerBP->GeneratedClass);
    TInlineComponentArray<UAudioComponent*> Channels(Manager);Expect(Channels.Num()==2,TEXT("Manager instantiates exactly Ambient and Music channels"));
    UAudioComponent* Ambience=nullptr;UAudioComponent* Music=nullptr;
    for(auto* C:Channels){if(C->GetFName()==TEXT("AC_Ambience"))Ambience=C;if(C->GetFName()==TEXT("AC_Music"))Music=C;}
    check(Ambience && Music);Ambience->SetVolumeMultiplier(.3f);Music->SetVolumeMultiplier(.25f);
    auto* One=W->SpawnActor<AAmbientSound>();auto* Two=W->SpawnActor<AAmbientSound>();
    auto* A=One->GetAudioComponent();auto* B=Two->GetAudioComponent();
    auto* Cue=NewObject<USoundCue>(W);auto* NewCue=NewObject<USoundCue>(W);
    A->SetSound(Cue);B->SetSound(Cue);A->SetVolumeMultiplier(.6f);B->SetVolumeMultiplier(.4f);
    Expect(!UVTGCutsceneAudioLibrary::GetFollowAmbientSequenceMute(A) && !UVTGCutsceneAudioLibrary::GetFollowAmbientSequenceMute(B),TEXT("Placed scene sound switch defaults OFF"));
    UVTGCutsceneAudioLibrary::SetFollowAmbientSequenceMute(A,true);
    auto* Foreground=W->SpawnActor<AAmbientSound>();Foreground->Tags.Add(TEXT("SequencerActor"));
    auto* ForegroundAudio=Foreground->GetAudioComponent();ForegroundAudio->SetVolumeMultiplier(.9f);UVTGCutsceneAudioLibrary::SetFollowAmbientSequenceMute(ForegroundAudio,true);
    Expect(UVTGCutsceneAudioLibrary::GetFollowAmbientSequenceMute(A) && !UVTGCutsceneAudioLibrary::GetFollowAmbientSequenceMute(B) && A->Sound==B->Sound,TEXT("Shared Cue supports independent instance switches without asset mutation"));
    auto* Seq=NewObject<ULevelSequence>(W);Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,240000);
    auto* Settings=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Seq,true);
    Expect(Settings && !Settings->IsEditorOnly() && Settings->Preset==EVTGCutsceneAudioPreset::KeepWorld,TEXT("Runtime/cooked asset user data defaults to keep existing audio"));
    Settings->Preset=EVTGCutsceneAudioPreset::MuteWorldBeds;Settings->FadeSeconds=0;
    auto* Duplicate=DuplicateObject<ULevelSequence>(Seq,W);
    Expect(UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Duplicate)->Preset==EVTGCutsceneAudioPreset::MuteWorldBeds,TEXT("Sequence audio profile is copied as persistent asset user data"));
    FMovieSceneSequencePlaybackSettings Playback;ALevelSequenceActor* SequenceActor=nullptr;
    auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,Playback,SequenceActor);check(Player);
    Player->Play();State->Tick(.1f);
    Expect(Ambience->VolumeMultiplier==0 && Music->VolumeMultiplier==0 && A->VolumeMultiplier==0 && FMath::IsNearlyEqual(B->VolumeMultiplier,.4f),TEXT("Actual Sequence mutes manager beds and opted-in Cue only"));
    Expect(FMath::IsNearlyEqual(ForegroundAudio->VolumeMultiplier,.9f),TEXT("Sequence-spawned foreground sound is protected even if its template copied a scene flag"));
    Expect(A->Sound==Cue && B->Sound==Cue,TEXT("Mute never swaps the scene sound asset"));
    Player->Pause();State->Tick(.1f);
    Expect(A->VolumeMultiplier==0,TEXT("Paused Sequence retains its mix"));
    // Music may change stage during a cutscene: retain the latest source and authored volume.
    Music->SetSound(NewCue);Music->SetVolumeMultiplier(.8f);State->Tick(.1f);
    Expect(Music->VolumeMultiplier==0,TEXT("New music stage stays under the active cutscene gain"));
    UVTGCutsceneAudioLibrary::SetFollowAmbientSequenceMute(A,false);State->Tick(.1f);
    Expect(FMath::IsNearlyEqual(A->VolumeMultiplier,.6f),TEXT("Unchecking instance during cutscene restores its current base"));
    Player->Stop();State->Tick(.1f);
    Expect(FMath::IsNearlyEqual(Music->VolumeMultiplier,.8f) && Music->Sound==NewCue && FMath::IsNearlyEqual(Ambience->VolumeMultiplier,.3f),TEXT("Stop/interruption restores current stage without replaying old music"));
    UVTGCutsceneAudioLibrary::SetFollowAmbientSequenceMute(A,true);
    auto* FocusSeq=NewObject<ULevelSequence>(W);FocusSeq->Initialize();FocusSeq->GetMovieScene()->SetPlaybackRange(0,240000);
    auto* Focus=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(FocusSeq,true);Focus->Preset=EVTGCutsceneAudioPreset::DialogueFocus;Focus->FadeSeconds=0;
    ALevelSequenceActor* FocusActor=nullptr;auto* FocusPlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(W,FocusSeq,Playback,FocusActor);check(FocusPlayer);
    FocusPlayer->Play();State->Tick(.1f);
    Expect(FMath::IsNearlyEqual(A->VolumeMultiplier,.15f) && FMath::IsNearlyEqual(Ambience->VolumeMultiplier,.075f) && Music->VolumeMultiplier==0,TEXT("Dialogue-focus preset ducks ambient and fades music"));
    Player->Play();State->Tick(.1f);Expect(A->VolumeMultiplier==0,TEXT("Overlapping restrictive Sequence takes priority"));
    Player->Stop();State->Tick(.1f);Expect(FMath::IsNearlyEqual(A->VolumeMultiplier,.15f),TEXT("Ending one Sequence cannot release another Sequence's duck"));
    FocusPlayer->GoToEndAndStop();State->Tick(.1f);Expect(FMath::IsNearlyEqual(A->VolumeMultiplier,.6f),TEXT("Skip-to-end releases mix"));
    TMap<UAudioComponent*,float> Targets;Targets.Add(A,0.f);
    State->ApplyTargets(Targets,.1f,.4f);Expect(A->VolumeMultiplier>0 && A->VolumeMultiplier<.6f,TEXT("Gain transition is smooth"));
    State->ApplyTargets(Targets,.4f,.4f);Expect(A->VolumeMultiplier==0,TEXT("Fade reaches full mute"));
    State->ApplyTargets(TMap<UAudioComponent*,float>(),.4f,.4f);Expect(FMath::IsNearlyEqual(A->VolumeMultiplier,.6f),TEXT("Fade release reaches original base"));
    State->ApplyTargets(Targets,.5f,.1f);A->SetVolumeMultiplier(.7f);State->ApplyTargets(Targets,.1f,0);
    State->RestoreAll();Expect(FMath::IsNearlyEqual(A->VolumeMultiplier,.7f),TEXT("World teardown restores latest external volume"));
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return true;
}
