#include "Audio/VTGNarrativeAudio.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "NarrativeComponent.h"
#include "NarrativeEvent.h"
#include "Dialogue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeLooping.h"
#include "UObject/UnrealType.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

bool VTGVerifyIndependentAudio()
{
    auto Expect=[](bool OK,const TCHAR* Message) { checkf(OK,TEXT("%s"),Message); UE_LOG(LogTemp,Display,TEXT("INDEPENDENT_AUDIO PASS %s"),Message); };
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("IndependentAudioTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* State=World->GetSubsystem<UVTGNarrativeAudioSubsystem>(); check(State);
    auto* Pawn=World->SpawnActor<APawn>();
    auto* Component=NewObject<UNarrativeComponent>(Pawn);
    auto* OtherOwner=NewObject<UNarrativeComponent>(Pawn);
    auto* Dialogue=NewObject<UDialogue>(Component);
    auto* OtherDialogue=NewObject<UDialogue>(Component);
    auto* Current=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("CurrentDialogue")); check(Current);
    Current->SetObjectPropertyValue_InContainer(Component,Dialogue);
    auto* A=NewObject<USoundWave>(World); A->Duration=1.f; A->bLooping=true;
    auto* B=NewObject<USoundWave>(World); B->Duration=2.f;
    auto Play=[&](USoundBase* Sound,FName ID,bool AutoStop=true) { State->Play(Component,Component,Sound,ID,true,AutoStop,0.f,1.f,0.f); };
    Play(A,TEXT("N")); Play(B,TEXT("M"));
    State->Stop(Component,B,TEXT("MissingTag"),0.f);
    State->Stop(OtherOwner,A,TEXT("N"),0.f);
    State->Stop(Component,nullptr,NAME_None,0.f);
    Expect(State->Sounds.Num()==2,TEXT("Missing tag never falls back to supplied audio; wrong owner and empty request are safe"));
    auto* OriginalM=State->Sounds[1].Audio.Get();
    State->Stop(Component,B,TEXT("N"),0.f);
    Expect(State->Sounds.Num()==1 && State->Sounds[0].Audio==OriginalM,TEXT("Tag N takes priority over audio B and leaves tag M untouched"));
    Play(B,TEXT("Second"));
    Expect(State->Sounds.Num()==2,TEXT("Same sound with distinct IDs coexists"));
    Play(B,TEXT("Second"));
    Expect(State->Sounds.Num()==2,TEXT("Replay replaces only exact key"));
    State->Stop(Component,B,TEXT("Second"),0.f);
    Current->SetObjectPropertyValue_InContainer(Component,OtherDialogue);
    Play(A,TEXT("Later"));
    Play(B,TEXT("Persistent"),false);
    Component->OnDialogueFinished.Broadcast(Dialogue,true);
    Expect(State->Sounds.Num()==2,TEXT("Old dialogue completion preserves newer dialogue and opted-out playback"));
    Component->OnDialogueFinished.Broadcast(OtherDialogue,false);
    Expect(State->Sounds.Num()==1 && State->Sounds[0].ID==TEXT("Persistent"),TEXT("Dialogue finish stops its sounds, including loops; opt-out survives"));
    State->Finished(State->Sounds[0].Audio);
    Expect(State->Sounds.IsEmpty(),TEXT("Natural completion releases tracked component"));
    auto* Once=CastChecked<USoundCue>(State->MakePlaybackSound(State,A,false));
    auto* Loop=CastChecked<USoundCue>(State->MakePlaybackSound(State,A,true));
    Expect(!CastChecked<USoundNodeWavePlayer>(Once->FirstNode)->bLooping && CastChecked<USoundNodeWavePlayer>(Loop->FirstNode)->bLooping && A->bLooping,TEXT("Wave loop switch overrides playback without editing source"));
    auto* SourceCue=NewObject<USoundCue>(World);
    auto* SourcePlayer=NewObject<USoundNodeWavePlayer>(SourceCue); SourcePlayer->SetSoundWave(A); SourcePlayer->bLooping=true;
    auto* SourceLoop=NewObject<USoundNodeLooping>(SourceCue); SourceLoop->ChildNodes.Add(SourcePlayer); SourceLoop->bLoopIndefinitely=true; SourceCue->FirstNode=SourceLoop;
    auto* CueOnce=CastChecked<USoundCue>(State->MakePlaybackSound(State,SourceCue,false));
    Expect(CueOnce->FirstNode!=SourcePlayer && !CastChecked<USoundNodeWavePlayer>(CueOnce->FirstNode)->bLooping && SourcePlayer->bLooping && SourceCue->FirstNode==SourceLoop,TEXT("Cue loop normalization is private; original graph unchanged"));
    auto* CueLoop=CastChecked<USoundCue>(State->MakePlaybackSound(State,SourceCue,true));
    Expect(Cast<USoundNodeLooping>(CueLoop->FirstNode)!=nullptr,TEXT("Cue loop switch repeats whole normalized graph"));
    {
        TGuardValue<bool> RuntimeGuard(GIsEditor,false);
        auto* RuntimeOnce=State->MakePlaybackSound(State,A,false);
        auto* RuntimeLoop=State->MakePlaybackSound(State,A,true);
        Expect(FMath::IsNearlyEqual(RuntimeOnce->GetDuration(),1.f) && RuntimeLoop->IsLooping(),TEXT("Duration and looping remain correct outside editor mode"));
    }

    TGuardValue<bool> Guard(GAllowActorScriptExecutionInEditor,true);
    const FString Root=TEXT("/Game/LevelNSequence/LevelAudio/NE/NE_VTG_Audio_");
    auto* PlayBP=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("PlayOneShot")));
    auto* StopBP=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("StopOneShot"))); check(PlayBP && StopBP);
    for(auto* BP:{PlayBP,StopBP}) { FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log); Expect(Log.NumErrors==0 && Log.NumWarnings==0,TEXT("Existing NE asset compiles cleanly")); }
    auto* PlayEvent=NewObject<UNarrativeEvent>(Component,PlayBP->GeneratedClass);
    auto* StopEvent=NewObject<UNarrativeEvent>(Component,StopBP->GeneratedClass);
    FindFProperty<FObjectPropertyBase>(PlayEvent->GetClass(),TEXT("NewSound"))->SetObjectPropertyValue_InContainer(PlayEvent,A);
    Expect(FindFProperty<FBoolProperty>(PlayEvent->GetClass(),TEXT("StopWhenDialogueFinished"))->GetPropertyValue_InContainer(PlayEvent) && !FindFProperty<FBoolProperty>(PlayEvent->GetClass(),TEXT("Loop"))->GetPropertyValue_InContainer(PlayEvent),TEXT("Defaults: loop off, dialogue-end stop on"));
    PlayEvent->ExecuteEvent(Pawn,nullptr,Component);
    Expect(State->Sounds.Num()==1 && State->Sounds[0].Source==A,TEXT("Actual Play NE works with no BPLM or AudioManager"));
    StopEvent->ExecuteEvent(Pawn,nullptr,Component);
    Expect(State->Sounds.Num()==1,TEXT("Unconfigured actual Stop NE is safe no-op"));
    FindFProperty<FObjectPropertyBase>(StopEvent->GetClass(),TEXT("SoundToStop"))->SetObjectPropertyValue_InContainer(StopEvent,A);
    StopEvent->ExecuteEvent(Pawn,nullptr,Component);
    Expect(State->Sounds.IsEmpty(),TEXT("Actual Stop NE stops its matching sound"));
    Expect(FindFProperty<FProperty>(StopBP->GeneratedClass,TEXT("SoundToStop"))->HasAnyPropertyFlags(CPF_AdvancedDisplay),TEXT("Stop audio fallback is folded into advanced properties"));
    Play(A,TEXT("Replace")); Play(B,TEXT("Replace"));
    Expect(State->Sounds.Num()==1 && State->Sounds[0].Source==B,TEXT("Same tag replaces previous sound even with a different source"));
    State->Stop(Component,nullptr,TEXT("Replace"),0.f);
    Expect(State->Sounds.IsEmpty(),TEXT("Tag-only stop does not require an audio source"));
    Play(A,TEXT("First")); Play(A,TEXT("Second")); Play(B,TEXT("Other"));
    State->Stop(Component,A,NAME_None,0.f);
    Expect(State->Sounds.Num()==1 && State->Sounds[0].Source==B,TEXT("No-tag audio fallback stops matching source only"));
    State->Stop(Component,nullptr,TEXT("Other"),0.f);

    auto* External=NewObject<UAudioComponent>(Pawn); External->bAutoActivate=false;
    External->SetSound(A); External->RegisterComponentWithWorld(World);
    Expect(!UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(External,Component,NAME_None),TEXT("External registration requires a named tag"));
    Expect(UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(External,Component,TEXT("External")),TEXT("Externally created audio component can register for NE Stop"));
    Expect(UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(External,Component,TEXT("External")) && State->Sounds.Num()==1,TEXT("Re-registering same external component is idempotent"));
    FindFProperty<FNameProperty>(StopEvent->GetClass(),TEXT("PlaybackID"))->SetPropertyValue_InContainer(StopEvent,TEXT("External"));
    FindFProperty<FObjectPropertyBase>(StopEvent->GetClass(),TEXT("SoundToStop"))->SetObjectPropertyValue_InContainer(StopEvent,nullptr);
    StopEvent->ExecuteEvent(Pawn,nullptr,Component);
    Expect(State->Sounds.IsEmpty() && IsValid(External) && External->IsRegistered(),TEXT("Actual NE stops registered external audio without destroying owner component"));
    UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(External,Component,TEXT("External"));
    External->SetSound(B);
    StopEvent->ExecuteEvent(Pawn,nullptr,Component);
    Expect(State->Sounds.IsEmpty() && External->Sound==B && External->IsRegistered(),TEXT("Reused external component's different sound is not stopped"));
    UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(External,Component,TEXT("External"));
    UVTGNarrativeAudioLibrary::UnregisterAudioForNarrativeStop(External);
    Expect(State->Sounds.IsEmpty() && External->IsRegistered(),TEXT("Unregister releases only tracking"));
    UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(External,Component,TEXT("External"));
    State->Finished(External);
    Expect(State->Sounds.IsEmpty() && External->IsRegistered(),TEXT("External natural completion does not destroy persistent component"));
    External->DestroyComponent();
    Play(A,TEXT("Teardown"),false);
    State->Deinitialize();
    Expect(State->Sounds.IsEmpty(),TEXT("World teardown stops even persistent loops"));
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
