#include "Audio/VTGCutsceneAudio.h"
#include "Components/AudioComponent.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

const FName UVTGCutsceneAudioSubsystem::FollowAmbientTag(TEXT("VTG.FollowAmbientSequenceMute"));
bool UVTGCutsceneAudioSubsystem::DoesSupportWorldType(EWorldType::Type Type) const {return Type==EWorldType::Game || Type==EWorldType::PIE;}
TStatId UVTGCutsceneAudioSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UVTGCutsceneAudioSubsystem,STATGROUP_Tickables);}
void UVTGCutsceneAudioSubsystem::ApplyTargets(const TMap<UAudioComponent*,float>& Targets,float Dt,float FadeSeconds)
{
    for(auto& Pair:Targets)if(IsValid(Pair.Key) && !Controlled.Contains(Pair.Key) && Pair.Value<1.f)
    {
        FGainState State;State.Base=State.LastApplied=Pair.Key->VolumeMultiplier;Controlled.Add(Pair.Key,State);
    }
    for(auto It=Controlled.CreateIterator();It;++It)
    {
        auto* Audio=It.Key().Get();if(!IsValid(Audio)){It.RemoveCurrent();continue;}
        auto& State=It.Value();
        // Stage changes and authored component-volume edits become the new base, even while muted.
        if(!FMath::IsNearlyEqual(Audio->VolumeMultiplier,State.LastApplied,.000001f)) State.Base=Audio->VolumeMultiplier;
        const float* Requested=Targets.Find(Audio);const float Target=Requested?FMath::Clamp(*Requested,0.f,1.f):1.f;
        if(!FMath::IsNearlyEqual(Target,State.Target))
        {State.Start=State.Gain;State.Target=Target;State.Elapsed=0;State.Duration=FMath::Max(0.f,FadeSeconds);}
        State.Elapsed+=Dt;
        State.Gain=State.Duration>0?FMath::Lerp(State.Start,State.Target,FMath::Clamp(State.Elapsed/State.Duration,0.f,1.f)):State.Target;
        State.LastApplied=State.Base*State.Gain;Audio->SetVolumeMultiplier(State.LastApplied);
        if(State.Target==1.f && FMath::IsNearlyEqual(State.Gain,1.f))It.RemoveCurrent();
    }
}
void UVTGCutsceneAudioSubsystem::Tick(float DeltaTime)
{
    if(!GetWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    ScanElapsed+=DeltaTime;if(ScanElapsed<.05f)return;
    const float Dt=ScanElapsed;ScanElapsed=0;
    float Ambience=1.f,Music=1.f,Fade=0.f;bool HasOverride=false;
    // Paused players still own their mix; stopped, skipped and destroyed players release it.
    for(TObjectIterator<ULevelSequencePlayer> It;It;++It)if(It->GetWorld()==GetWorld() && (It->IsPlaying() || It->IsPaused()))
        if(auto* Seq=Cast<ULevelSequence>(It->GetSequence()))
            if(auto* Settings=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Seq))
            {
                if(Settings->Preset==EVTGCutsceneAudioPreset::KeepWorld)continue;
                HasOverride=true;
                Fade=FMath::Max(Fade,Settings->FadeSeconds);
                Ambience=FMath::Min(Ambience,Settings->Preset==EVTGCutsceneAudioPreset::DialogueFocus?.25f:0.f);
                Music=0.f;
            }
    TMap<UAudioComponent*,float> Targets;
    if(Ambience<1.f || Music<1.f)
    {
        auto* ManagerClass=FindObject<UClass>(nullptr,TEXT("/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager.BP_VTG_AudioManager_C"));
        for(TObjectIterator<UAudioComponent> It;It;++It)if(It->GetWorld()==GetWorld() && It->IsRegistered())
        {
            auto* Owner=It->GetOwner();
            if(ManagerClass && Owner && Owner->IsA(ManagerClass))
            {
                if(It->GetFName()==TEXT("AC_Ambience"))Targets.Add(*It,Ambience);
                else if(It->GetFName()==TEXT("AC_Music"))Targets.Add(*It,Music);
            }
            // Engine tags Sequence spawnables. A copied scene flag must not mute foreground cutscene actors.
            else if(It->ComponentHasTag(FollowAmbientTag) && (!Owner || !Owner->ActorHasTag(TEXT("SequencerActor"))))Targets.Add(*It,Ambience);
        }
    }
    if(HasOverride)LastFadeSeconds=Fade;
    ApplyTargets(Targets,Dt,HasOverride?Fade:LastFadeSeconds);
}
void UVTGCutsceneAudioSubsystem::RestoreAll()
{
    for(auto& Pair:Controlled)if(auto* Audio=Pair.Key.Get())
    {
        auto& State=Pair.Value;
        if(FMath::IsNearlyEqual(Audio->VolumeMultiplier,State.LastApplied,.000001f))Audio->SetVolumeMultiplier(State.Base);
    }
    Controlled.Empty();
}
void UVTGCutsceneAudioSubsystem::Deinitialize(){RestoreAll();Super::Deinitialize();}
UVTGCutsceneAudioSettings* UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(ULevelSequence* Sequence,bool CreateIfMissing)
{
    if(!IsValid(Sequence))return nullptr;
    auto* Settings=Cast<UVTGCutsceneAudioSettings>(Sequence->GetAssetUserDataOfClass(UVTGCutsceneAudioSettings::StaticClass()));
    if(!Settings && CreateIfMissing){Settings=NewObject<UVTGCutsceneAudioSettings>(Sequence,NAME_None,RF_Transactional);Sequence->AddAssetUserData(Settings);}
    return Settings;
}
void UVTGCutsceneAudioLibrary::SetFollowAmbientSequenceMute(UAudioComponent* Audio,bool Enabled)
{
    if(!IsValid(Audio))return;
    if(Enabled)Audio->ComponentTags.AddUnique(UVTGCutsceneAudioSubsystem::FollowAmbientTag);
    else Audio->ComponentTags.Remove(UVTGCutsceneAudioSubsystem::FollowAmbientTag);
}
bool UVTGCutsceneAudioLibrary::GetFollowAmbientSequenceMute(const UAudioComponent* Audio)
{return IsValid(Audio) && Audio->ComponentHasTag(UVTGCutsceneAudioSubsystem::FollowAmbientTag);}
