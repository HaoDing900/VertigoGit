#include "Audio/VTGSpatialLoopComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundAttenuation.h"

UVTGSpatialLoopComponent::UVTGSpatialLoopComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=.05f;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void UVTGSpatialLoopComponent::BeginPlay()
{
    Super::BeginPlay();
    LastLocation=GetOwner()->GetActorLocation();
    RefreshPlayback();
}
float UVTGSpatialLoopComponent::MovementAmount(float Speed,float DeadZone,float MaxSpeed)
{
    return FMath::Clamp((Speed-DeadZone)/FMath::Max(1.f,MaxSpeed-DeadZone),0.f,1.f);
}
void UVTGSpatialLoopComponent::UpdateMovement(float Speed,float DeltaTime)
{
    const float Target=bReactToMovement?MovementAmount(Speed,MovementDeadZone,FullSpeed):0.f;
    MovementBlend=FMath::FInterpTo(MovementBlend,Target,DeltaTime,ResponseSpeed);
    if(Audio)
    {
        Audio->SetVolumeMultiplier(FMath::Lerp(IdleVolume,MovingVolume,MovementBlend));
        Audio->SetPitchMultiplier(FMath::Lerp(IdlePitch,MovingPitch,MovementBlend));
    }
}
void UVTGSpatialLoopComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    const FVector Position=GetOwner()->GetActorLocation();
    float Speed=DeltaTime>SMALL_NUMBER?FVector::Distance(Position,LastLocation)/DeltaTime:0.f;
    LastLocation=Position;
    // Ignore teleports; use displacement rather than velocity so timeline-driven Follow/GoTo also works.
    if(Speed>FMath::Max(FullSpeed*8.f,3000.f)) Speed=0.f;
    UpdateMovement(Speed,DeltaTime);
}
void UVTGSpatialLoopComponent::RefreshPlayback()
{
    if(!GetOwner() || !GetWorld() || !GetWorld()->IsGameWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer) return;
    if(!bLoopEnabled || MutedDialogue.IsValid() || !Sound)
    {
        if(Audio) Audio->Stop();
        return;
    }
    if(!Audio)
    {
        LoopCue=NewObject<USoundCue>(this);
        LoopCue->VolumeMultiplier=1.f;
        LoopCue->SoundClassObject=Sound->SoundClassObject;
        LoopCue->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;
        auto* Player=NewObject<USoundNodeWavePlayer>(LoopCue);
        Player->SetSoundWave(Sound); Player->bLooping=true;
        LoopCue->FirstNode=Player; LoopCue->CacheAggregateValues();
        LoopCue->Duration=INDEFINITELY_LOOPING_DURATION;
        Audio=NewObject<UAudioComponent>(GetOwner());
        Audio->bAutoActivate=false; Audio->bAutoDestroy=false;
        Audio->bAllowSpatialization=true; Audio->bIsUISound=false;
        Audio->SetupAttachment(GetOwner()->GetRootComponent());
        Audio->AttenuationSettings=Attenuation;
        Audio->SetSound(LoopCue);
        Audio->SetVolumeMultiplier(IdleVolume);
        Audio->SetPitchMultiplier(IdlePitch);
        Audio->RegisterComponent();
    }
    if(!Audio->IsPlaying()) Audio->Play();
}
void UVTGSpatialLoopComponent::SetLoopEnabled(bool Enabled)
{
    bLoopEnabled=Enabled;
    RefreshPlayback();
}
void UVTGSpatialLoopComponent::SuspendForDialogue(UNarrativeComponent* Narrative)
{
    if(!Narrative || !Narrative->GetCurrentDialogue()) return;
    if(BoundNarrative.IsValid()) BoundNarrative->OnDialogueFinished.RemoveAll(this);
    BoundNarrative=Narrative; MutedDialogue=Narrative->GetCurrentDialogue();
    Narrative->OnDialogueFinished.AddUniqueDynamic(this,&UVTGSpatialLoopComponent::DialogueEnded);
    RefreshPlayback();
}
void UVTGSpatialLoopComponent::DialogueEnded(UDialogue* Dialogue,bool)
{
    if(MutedDialogue.Get()!=Dialogue) return;
    MutedDialogue.Reset();
    if(BoundNarrative.IsValid()) BoundNarrative->OnDialogueFinished.RemoveAll(this);
    BoundNarrative.Reset();
    RefreshPlayback();
}
void UVTGSpatialLoopComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(BoundNarrative.IsValid()) BoundNarrative->OnDialogueFinished.RemoveAll(this);
    if(Audio) { Audio->Stop(); Audio->DestroyComponent(); Audio=nullptr; }
    LoopCue=nullptr;
    Super::EndPlay(Reason);
}
void UVTGSpatialAudioLibrary::SetBelloRepairLoop(AActor* Bello,bool Helped,USoundWave* RepairSound,USoundAttenuation* Attenuation)
{
    if(!IsValid(Bello)) return;
    auto* Loop=Bello->FindComponentByClass<UVTGSpatialLoopComponent>();
    if(!Loop)
    {
        if(Helped) return;
        Loop=NewObject<UVTGSpatialLoopComponent>(Bello,TEXT("BelloRepairAudio"));
        Loop->Sound=RepairSound; Loop->Attenuation=Attenuation;
        Loop->IdleVolume=.35f; Loop->MovingVolume=.35f;
        Loop->IdlePitch=1.f; Loop->MovingPitch=1.f;
        Bello->AddInstanceComponent(Loop); Loop->RegisterComponent();
    }
    Loop->SetLoopEnabled(!Helped);
}
void UVTGSpatialAudioLibrary::SuspendBelloRepairForDialogue(AActor* Bello,UNarrativeComponent* Narrative)
{
    if(IsValid(Bello))
        if(auto* Loop=Bello->FindComponentByClass<UVTGSpatialLoopComponent>()) Loop->SuspendForDialogue(Narrative);
}
