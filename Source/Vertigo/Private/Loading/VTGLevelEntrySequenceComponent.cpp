#include "Loading/VTGLevelEntrySequenceComponent.h"
#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Save/VTGSaveCoordinator.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"

UVTGLevelEntrySequenceComponent::UVTGLevelEntrySequenceComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    PlaybackSettings.bDisableMovementInput=true;
    PlaybackSettings.bDisableLookAtInput=true;
    PlaybackSettings.bHideHud=true;
    PlaybackSettings.bRestoreState=true;
}
void UVTGLevelEntrySequenceComponent::BeginPlay()
{
    Super::BeginPlay();
    if(auto* GI=GetWorld()->GetGameInstance())
    {
        Save=GI->GetSubsystem<UVTGSaveCoordinator>();
        bResumingSave=IsSaveResume();
        if(Save)Save->OnSlotLoaded.AddUniqueDynamic(this,&UVTGLevelEntrySequenceComponent::OnSlotLoaded);
    }
    // Let the manager's BeginPlay/OnStageBegin initialization run before automatic playback.
    if(bPlayOnBeginPlay)AutoPlayTimer=GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){PlayEntry();}));
}
bool UVTGLevelEntrySequenceComponent::IsSaveResume() const
{
    return bResumingSave || (Save && (Save->HasPendingWorldRestore() || !Save->GetResumeCheckpoint().IsNone()));
}
bool UVTGLevelEntrySequenceComponent::PlayEntry()
{
    if(bRequested || !EntrySequence || !GetWorld())return false;
    if(bSkipOnSaveResume && IsSaveResume()){OnFinished.Broadcast(true);return false;}
    return PlayEntryPlayer(CreateEntryPlayer());
}
UMovieSceneSequencePlayer* UVTGLevelEntrySequenceComponent::CreateEntryPlayer()
{
    if(bRequested || !EntrySequence || !GetWorld())return nullptr;
    if(OwnedActor){OwnedActor->Destroy();OwnedActor=nullptr;}
    auto Settings=PlaybackSettings;Settings.bAutoPlay=false;
    ALevelSequenceActor* Actor=nullptr;
    auto* NewPlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(),EntrySequence,Settings,Actor);
    OwnedActor=Actor;
    Player=NewPlayer;
    return NewPlayer;
}
bool UVTGLevelEntrySequenceComponent::PlayEntryPlayer(UMovieSceneSequencePlayer* InPlayer)
{
    if(bRequested || !IsValid(InPlayer) || InPlayer->GetWorld()!=GetWorld() || InPlayer->IsPlaying())return false;
    if(!Save && GetWorld()->GetGameInstance())Save=GetWorld()->GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>();
    if(bSkipOnSaveResume && IsSaveResume()){OnFinished.Broadcast(true);return false;}
    Player=InPlayer;bRequested=true;bStarted=false;++RequestSerial;
    Player->OnFinished.AddUniqueDynamic(this,&UVTGLevelEntrySequenceComponent::OnSequenceFinished);
    Player->OnStop.AddUniqueDynamic(this,&UVTGLevelEntrySequenceComponent::OnSequenceStopped);
    if(auto* GI=GetWorld()->GetGameInstance())Loading=GI->GetSubsystem<UVTGMediaLoadingPageSystem>();
    if(StartPolicy==EVTGEntryStartPolicy::Immediate || !Loading || !Loading->IsLoadingPageVisible()
        || (StartPolicy==EVTGEntryStartPolicy::WithLoadingReveal && Loading->HasWorldRevealStarted()))
    {StartNow();return true;}
    // Do not Play/Pause here: no audio, event tracks or gameplay callbacks run under the cover.
    Loading->OnWorldRevealStarted.AddUObject(this,&UVTGLevelEntrySequenceComponent::OnReveal);
    Loading->OnLoadingPageHidden.AddUObject(this,&UVTGLevelEntrySequenceComponent::OnHidden);
    Loading->OnLoadingCancelled.AddUObject(this,&UVTGLevelEntrySequenceComponent::OnCancelled);
    return true;
}
void UVTGLevelEntrySequenceComponent::UnbindLoading()
{
    if(Loading){Loading->OnWorldRevealStarted.RemoveAll(this);Loading->OnLoadingPageHidden.RemoveAll(this);Loading->OnLoadingCancelled.RemoveAll(this);}
    Loading=nullptr;
}
void UVTGLevelEntrySequenceComponent::StartNow()
{
    if(!bRequested || bStarted || !IsValid(Player))return;
    UnbindLoading();bStarted=true;
    Player->Play();
    OnStarted.Broadcast();
}
void UVTGLevelEntrySequenceComponent::OnReveal(){if(StartPolicy==EVTGEntryStartPolicy::WithLoadingReveal)StartNow();}
void UVTGLevelEntrySequenceComponent::OnHidden(){StartNow();}
void UVTGLevelEntrySequenceComponent::OnCancelled()
{
    // Cancelled travel is not a completed cinematic; do not trigger the manager's story continuation.
    UnbindLoading();
    if(Player){Player->OnFinished.RemoveAll(this);Player->OnStop.RemoveAll(this);}
    bRequested=bStarted=false;
}
void UVTGLevelEntrySequenceComponent::Finish(bool Skipped)
{
    if(!bRequested)return;
    UnbindLoading();
    if(Player){Player->OnFinished.RemoveAll(this);Player->OnStop.RemoveAll(this);}
    bRequested=bStarted=false;
    OnFinished.Broadcast(Skipped);
}
void UVTGLevelEntrySequenceComponent::OnSequenceFinished(){Finish(bSkipping);}
void UVTGLevelEntrySequenceComponent::OnSequenceStopped()
{
    // UE also emits OnStop immediately before OnFinished on a natural completion.
    // Give OnFinished priority; only a standalone stop is considered a skip.
    const uint32 Serial=RequestSerial;
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,Serial]()
    {
        if(bRequested && RequestSerial==Serial && Player && !Player->IsPlaying() && !Player->IsPaused())Finish(true);
    }));
}
void UVTGLevelEntrySequenceComponent::SkipEntry()
{
    if(!bRequested)return;
    if(bStarted && Player)
    {
        TGuardValue<bool> Guard(bSkipping,true);
        Player->GoToEndAndStop();
    }
    Finish(true);
}
void UVTGLevelEntrySequenceComponent::OnSlotLoaded(int32 Slot)
{
    bResumingSave=true;
    if(bSkipOnSaveResume && bRequested && !bStarted)Finish(true);
}
void UVTGLevelEntrySequenceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(AutoPlayTimer);
    if(Save)Save->OnSlotLoaded.RemoveAll(this);
    OnCancelled();
    if(OwnedActor){if(Player)Player->Stop();OwnedActor->Destroy();OwnedActor=nullptr;}
    Super::EndPlay(Reason);
}
