#include "FileHelpers.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "UObject/UnrealType.h"

int32 VTGProbeIntroPose()
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
    auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar"));
    ACharacter* Character=nullptr;
    for(TActorIterator<ACharacter> It(W);It;++It)if(It->GetClass()->GetName()==TEXT("BP_Player_Sa_C")){Character=*It;break;}
    if(!Character)return 1;
    auto* Mesh=Character->GetMesh();auto* Anim=Mesh->GetAnimInstance();
    auto* Flag=FindFProperty<FBoolProperty>(Anim->GetClass(),TEXT("IsCutscene"));if(!Flag)return 2;
    auto Report=[&](const TCHAR* Label){UE_LOG(LogTemp,Display,TEXT("INTRO POSE %s: IsCutscene=%d Montage=%s"),Label,Flag->GetPropertyValue_InContainer(Anim),*GetNameSafe(Anim->GetCurrentActiveMontage()));};
    struct {bool IsCutscene=true;} Params;
    if(auto* Fn=Anim->FindFunction(TEXT("SetIsCutscene")))Anim->ProcessEvent(Fn,&Params);
    Report(TEXT("after explicit interface setter(true), editor script allowed"));
    Flag->SetPropertyValue_InContainer(Anim,false);
    auto* Seq=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_IntroScene/LS_L2BarStreet_IntroScene"));
    ALevelSequenceActor* Actor=nullptr;
    auto* P=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,FMovieSceneSequencePlaybackSettings(),Actor);
    P->Play();P->Pause();
    P->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(1.f,EUpdatePositionMethod::Jump));
    Report(TEXT("nested intro at 1 second"));
    const bool EnteredCutscene=Flag->GetPropertyValue_InContainer(Anim);
    P->Stop();
    Report(TEXT("after sequence stop / state restoration"));
    return EnteredCutscene && !Flag->GetPropertyValue_InContainer(Anim) ? 0 : 3;
}
