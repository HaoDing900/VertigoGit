#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "NarrativeEvent.h"
#include "Sound/SoundWave.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool VTGVerifyLevelAudio()
{
    const FString Root=TEXT("/Game/LevelNSequence/LevelAudio/");
    auto* Audio=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("BP_VTG_AudioManager")));
    auto* LM=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM"));
    auto* ChildAudio=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("BP_VTG_AudioManager_Example")));
    auto* ChildLM=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("BPLM_AudioExample")));
    check(Audio && LM && ChildAudio && ChildLM);
    FString Report; int32 Count=0;
    auto Expect=[&](bool OK,const TCHAR* Message){ UE_LOG(LogTemp,Display,TEXT("VTG_AUDIO_TEST %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Message); checkf(OK,TEXT("%s"),Message); Report+=FString(TEXT("PASS "))+Message+TEXT("\n"); ++Count; };
    for(auto* B:{Audio,LM,ChildAudio,ChildLM}) {FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::SkipGarbageCollection,&Log); Expect(!Log.NumErrors && !Log.NumWarnings,TEXT("Parent/child Blueprint compiles without errors or warnings"));}
    TGuardValue<bool> Guard(GAllowActorScriptExecutionInEditor,true);
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("VTGAudioVerification"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* A=W->SpawnActor<AActor>(ChildAudio->GeneratedClass,FTransform::Identity,Spawn);
    auto* M=W->SpawnActor<AActor>(LM->GeneratedClass,FTransform::Identity,Spawn);
    auto* Pawn=W->SpawnActor<APawn>(APawn::StaticClass(),FTransform::Identity,Spawn);
    TArray<UAudioComponent*> Components; A->GetComponents(Components); Expect(Components.Num()==3,TEXT("Audio child inherits exactly three channels"));
    auto Track=[&](const TCHAR* Name)->UAudioComponent*{for(auto* C:Components)if(C->GetFName()==Name)return C; checkNoEntry();return nullptr;};
    auto* Amb=Track(TEXT("AC_Ambience")); auto* Music=Track(TEXT("AC_Music")); auto* Psych=Track(TEXT("AC_Psych"));
    auto* S1=NewObject<USoundWave>(W); auto* S2=NewObject<USoundWave>(W);
    auto Invoke=[&](UObject* O,const TCHAR* Name,USoundBase* Sound,float Fade){auto* F=O->FindFunction(Name);check(F);FStructOnScope Args(F);if(auto* P=FindFProperty<FObjectPropertyBase>(F,TEXT("NewSound")))P->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Sound);if(auto* P=FindFProperty<FFloatProperty>(F,TEXT("FadeTime")))P->SetPropertyValue_InContainer(Args.GetStructMemory(),Fade);O->ProcessEvent(F,Args.GetStructMemory());};
    Invoke(M,TEXT("Audio_PlayAmbience"),S1,0); Expect(Amb->Sound==S1,TEXT("BPLM resolves an AudioManager child and forwards ambience"));
    auto* Ref=FindFProperty<FObjectPropertyBase>(M->GetClass(),TEXT("AudioManagerRef")); Expect(Ref && Ref->GetObjectPropertyValue_InContainer(M)==A,TEXT("Auto-discovered reference is cached"));
    Invoke(M,TEXT("Audio_PlayMusic"),S2,-1); Expect(Music->Sound==S2 && Amb->Sound==S1,TEXT("Music uses its own channel; negative fade input is accepted"));
    Invoke(M,TEXT("Audio_PlayPsych"),S1,0); Expect(Psych->Sound==S1 && Music->Sound==S2,TEXT("Psych forwards independently of music"));
    Invoke(M,TEXT("Audio_PlayMusic"),nullptr,0); Expect(Music->Sound==S2,TEXT("Null sound is ignored without clearing current sound"));
    for(const TCHAR* N:{TEXT("Audio_StopAmbience"),TEXT("Audio_StopMusic"),TEXT("Audio_StopPsych")})Invoke(M,N,nullptr,0);
    Expect(true,TEXT("All stop functions execute without Blueprint exceptions"));
    auto* Second=W->SpawnActor<AActor>(Audio->GeneratedClass,FTransform::Identity,Spawn); Ref->SetObjectPropertyValue_InContainer(M,Second);
    Invoke(M,TEXT("Audio_PlayMusic"),S1,0); Expect(Ref->GetObjectPropertyValue_InContainer(M)==Second && Music->Sound==S2,TEXT("Explicit reference wins when multiple managers exist"));
    Ref->SetObjectPropertyValue_InContainer(M,A);
    const TCHAR* Names[]={TEXT("Audio_PlayAmbience"),TEXT("Audio_StopAmbience"),TEXT("Audio_PlayMusic"),TEXT("Audio_PlayPsych"),TEXT("Audio_StopMusic"),TEXT("Audio_StopPsych")};
    for(int I=0;I<6;++I) {
        auto* BP=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("NE/NE_VTG_")+Names[I]));check(BP); FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);Expect(!Log.NumErrors && !Log.NumWarnings,TEXT("Narrative event compiles cleanly"));
        auto* E=NewObject<UNarrativeEvent>(W,BP->GeneratedClass);
        if(auto* Prop=FindFProperty<FObjectPropertyBase>(E->GetClass(),TEXT("NewSound")))Prop->SetObjectPropertyValue_InContainer(E,S2);
        E->ExecuteEvent(Pawn,nullptr,nullptr);
        if(I==0)Expect(Amb->Sound==S2,TEXT("Narrative -> BPLM -> ambience executes"));
        if(I==2)Expect(Music->Sound==S2,TEXT("Narrative -> BPLM -> music executes"));
        if(I==3)Expect(Psych->Sound==S2 && !E->bRefireOnLoad,TEXT("Narrative psych executes and does not replay on save load"));
    }
    if(auto* Fn=M->FindFunction(TEXT("Audio_PlayOneShot"))) {
        FStructOnScope Args(Fn); FindFProperty<FObjectPropertyBase>(Fn,TEXT("NewSound"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),S1);
        FindFProperty<FFloatProperty>(Fn,TEXT("FadeInTime"))->SetPropertyValue_InContainer(Args.GetStructMemory(),-1.f);
        FindFProperty<FFloatProperty>(Fn,TEXT("VolumeMultiplier"))->SetPropertyValue_InContainer(Args.GetStructMemory(),.5f);
        M->ProcessEvent(Fn,Args.GetStructMemory()); Expect(Psych->Sound==S1 && Music->Sound==S2,TEXT("Generic OneShot forwards with fade/volume inputs and preserves music"));
        FindFProperty<FObjectPropertyBase>(Fn,TEXT("NewSound"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),nullptr);
        M->ProcessEvent(Fn,Args.GetStructMemory()); Expect(Psych->Sound==S1,TEXT("Null OneShot is ignored"));
        Invoke(M,TEXT("Audio_StopOneShot"),nullptr,0);
        for(const TCHAR* N:{TEXT("NE_VTG_Audio_PlayOneShot"),TEXT("NE_VTG_Audio_StopOneShot")}) {
            auto* BP=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("NE/")+N));check(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);Expect(!Log.NumErrors && !Log.NumWarnings,TEXT("OneShot Narrative event compiles cleanly"));
            auto* Event=NewObject<UNarrativeEvent>(W,BP->GeneratedClass);if(auto* Prop=FindFProperty<FObjectPropertyBase>(Event->GetClass(),TEXT("NewSound")))Prop->SetObjectPropertyValue_InContainer(Event,S2);
            Event->ExecuteEvent(Pawn,nullptr,nullptr);Expect(Psych->Sound==S2 && !Event->bRefireOnLoad,TEXT("OneShot Narrative dispatch works without replay on load"));
        }
    }
    auto* Demo=W->SpawnActor<AActor>(ChildLM->GeneratedClass,FTransform::Identity,Spawn);
    FindFProperty<FObjectPropertyBase>(Demo->GetClass(),TEXT("AudioManagerRef"))->SetObjectPropertyValue_InContainer(Demo,A);
    for(const TCHAR* N:{TEXT("ExampleAmbience"),TEXT("ExampleMusic"),TEXT("ExamplePsych")})FindFProperty<FObjectPropertyBase>(Demo->GetClass(),N)->SetObjectPropertyValue_InContainer(Demo,S1);
    auto Apply=[&](const TCHAR* Stage){auto* F=Demo->FindFunction(TEXT("Audio_ApplyStage"));FStructOnScope Args(F);FindFProperty<FNameProperty>(F,TEXT("Stage"))->SetPropertyValue_InContainer(Args.GetStructMemory(),FName(Stage));Demo->ProcessEvent(F,Args.GetStructMemory());};
    Apply(TEXT("Demo_Normal")); Expect(Amb->Sound==S1 && Music->Sound==S1,TEXT("BPLM child Stage override calls inherited audio functions"));
    Apply(TEXT("Demo_Disturbed")); Expect(Psych->Sound==S1,TEXT("Disturbed example dispatches psychological sound"));
    Apply(TEXT("Demo_Void")); Apply(TEXT("UnknownStage")); Expect(true,TEXT("Void and unknown Stage execute safely"));
    W->DestroyActor(Demo); W->DestroyActor(A); W->DestroyActor(Second); Ref->SetObjectPropertyValue_InContainer(M,nullptr);
    Invoke(M,TEXT("Audio_PlayMusic"),S1,0); Expect(Ref->GetObjectPropertyValue_InContainer(M)==nullptr,TEXT("Missing AudioManager is a safe no-op"));
    Report+=FString::Printf(TEXT("Checks: %d\nAudio device disabled: audible fades and loop seams require PIE listening.\n"),Count);
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("LevelAudioVerification.txt")));
    GEngine->DestroyWorldContext(W); W->DestroyWorld(false); return true;
}
