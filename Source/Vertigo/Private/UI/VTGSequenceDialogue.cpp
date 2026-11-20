#include "UI/VTGSequenceDialogue.h"
#include "LevelSequenceActor.h"
#include "LevelSequence.h"
#include "UI/VTGSequenceSettings.h"
#include "LevelSequencePlayer.h"
#include "NarrativeComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Systems/MovieSceneEventSystems.h"
#include "EntitySystem/MovieSceneSpawnablesSystem.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"

UVTGSequenceDialogueEntrySystem::UVTGSequenceDialogueEntrySystem(const FObjectInitializer& Init):Super(Init)
{
    Phase=UE::MovieScene::ESystemPhase::Spawn;
    if(HasAnyFlags(RF_ClassDefaultObject))
    {
        DefineImplicitPrerequisite(GetClass(),UMovieScenePreSpawnEventSystem::StaticClass());
        DefineImplicitPrerequisite(GetClass(),UMovieSceneSpawnablesSystem::StaticClass());
    }
}
void UVTGSequenceDialogueEntrySystem::OnRun(FSystemTaskPrerequisites&,FSystemSubsequentTasks&)
{
    for(TObjectIterator<UVTGSequenceDialogueGate> It;It;++It)
        if(!It->IsTemplate() && !It->bPlaybackActive)
            if(auto* Player=It->BoundPlayer.Get();Player && Player->IsPlaying() && Player->GetEvaluationTemplate().GetEntitySystemLinker()==GetLinker())It->OnPlay();
}

const FName UVTGSequenceDialogueGate::KeepDialogueTag(TEXT("VTG.KeepNarrativeDialogueOnStart"));
const FName UVTGSequenceDialogueGate::EndDialogueTag(TEXT("VTG.EndNarrativeDialogueOnStart"));
bool UVTGSequenceDialogueGate::GetSequenceDefault(ULevelSequence* Sequence)
{
    const auto* Settings=Sequence?Cast<UVTGSequenceSettings>(Sequence->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass())):nullptr;
    return Settings && Settings->bEndNarrativeDialogueOnStart;
}
void UVTGSequenceDialogueGate::SetSequenceDefault(ULevelSequence* Sequence,bool Enabled)
{
    if(!Sequence)return;
    Sequence->Modify();
    auto* Settings=Cast<UVTGSequenceSettings>(Sequence->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass()));
    if(!Settings){Settings=NewObject<UVTGSequenceSettings>(Sequence,NAME_None,RF_Transactional);Sequence->AddAssetUserData(Settings);}
    Settings->Modify();Settings->bEndNarrativeDialogueOnStart=Enabled;Sequence->MarkPackageDirty();
}
bool UVTGSequenceDialogueGate::ShouldEndDialogue(const ALevelSequenceActor* Actor)
{
    if(!Actor || Actor->ActorHasTag(KeepDialogueTag) || Actor->ActorHasTag(TEXT("Narrative.DialogueShot")))return false;
    if(!Actor->ActorHasTag(EndDialogueTag) && !GetSequenceDefault(Actor->GetSequence()))return false;
    // Ownership is authoritative too: Narrative event sequences use a separate API from dialogue camera shots.
    // Do not let a missing tag make a dialogue terminate its own Sequence and restore player input mid-scene.
    for(TObjectIterator<UNarrativeComponent> It;It;++It)
        if(!It->IsTemplate() && It->GetWorld()==Actor->GetWorld() && It->GetDialogueLevelSequencePlayer()==Actor->SequencePlayer)return false;
    return true;
}
void UVTGSequenceDialogueGate::Install(ALevelSequenceActor* Actor)
{
    // GetSequencePlayer() deliberately returns null until an asset is assigned.
    // Bind the default subobject now, before deferred spawn / Auto Play can run.
    if(!IsValid(Actor) || !Actor->SequencePlayer)return;
    auto* Gate=Actor->FindComponentByClass<UVTGSequenceDialogueGate>();
    if(!Gate)
    {
        Gate=NewObject<UVTGSequenceDialogueGate>(Actor);
        Actor->AddInstanceComponent(Gate);Gate->RegisterComponent();
    }
    auto* Player=Actor->SequencePlayer.Get();
    if(Gate->BoundPlayer==Player)return;
    Gate->BoundPlayer=Player;
    Player->OnPlay.AddUniqueDynamic(Gate,&UVTGSequenceDialogueGate::OnPlay);
    Player->OnPlayReverse.AddUniqueDynamic(Gate,&UVTGSequenceDialogueGate::OnPlay);
    Player->OnStop.AddUniqueDynamic(Gate,&UVTGSequenceDialogueGate::OnPlaybackEnded);
    Player->OnFinished.AddUniqueDynamic(Gate,&UVTGSequenceDialogueGate::OnPlaybackEnded);
}
void UVTGSequenceDialogueGate::OnPlay()
{
    // Pause/resume and direction changes belong to the same playback.
    if(bPlaybackActive)return;
    bPlaybackActive=true;
    if(!ShouldEndDialogue(Cast<ALevelSequenceActor>(GetOwner())))return;
    TArray<TWeakObjectPtr<UNarrativeComponent>> ToExit;
    for(TObjectIterator<UNarrativeComponent> It;It;++It)
        if(!It->IsTemplate() && It->GetWorld()==GetWorld() && It->GetCurrentDialogue())
            if(auto* PC=It->GetOwningController();PC && PC->IsLocalController())ToExit.Add(*It);
    // Capture before broadcasts: a finish handler starting another dialogue isn't closed again.
    for(auto Comp:ToExit)if(Comp.IsValid())Comp->ForceExitDialogue();
}
void UVTGSequenceDialogueGate::OnPlaybackEnded(){bPlaybackActive=false;}
void UVTGSequenceDialogueGate::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* Player=BoundPlayer.Get())
    {Player->OnPlay.RemoveAll(this);Player->OnPlayReverse.RemoveAll(this);Player->OnStop.RemoveAll(this);Player->OnFinished.RemoveAll(this);}
    Super::EndPlay(Reason);
}
bool UVTGSequenceDialogueSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{return Type==EWorldType::Game || Type==EWorldType::PIE;}
void UVTGSequenceDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SpawnHandle=GetWorld()->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateUObject(this,&ThisClass::OnActorSpawned));
    LevelHandle=FWorldDelegates::LevelAddedToWorld.AddUObject(this,&ThisClass::OnLevelAdded);
}
void UVTGSequenceDialogueSubsystem::PostInitialize(){Super::PostInitialize();BindAll();}
void UVTGSequenceDialogueSubsystem::OnWorldBeginPlay(UWorld& World){Super::OnWorldBeginPlay(World);BindAll();}
void UVTGSequenceDialogueSubsystem::BindAll()
{for(TActorIterator<ALevelSequenceActor> It(GetWorld());It;++It)UVTGSequenceDialogueGate::Install(*It);}
void UVTGSequenceDialogueSubsystem::OnActorSpawned(AActor* Actor)
{if(auto* Sequence=Cast<ALevelSequenceActor>(Actor))UVTGSequenceDialogueGate::Install(Sequence);}
void UVTGSequenceDialogueSubsystem::OnLevelAdded(ULevel*,UWorld* World){if(World==GetWorld())BindAll();}
void UVTGSequenceDialogueSubsystem::Deinitialize()
{
    if(GetWorld())GetWorld()->RemoveOnActorPreSpawnInitialization(SpawnHandle);
    FWorldDelegates::LevelAddedToWorld.Remove(LevelHandle);
    Super::Deinitialize();
}
