#include "Audio/VTGNarrativeAudio.h"
#include "Components/AudioComponent.h"
#include "Dialogue.h"
#include "NarrativeComponent.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeLooping.h"

USoundBase* UVTGNarrativeAudioSubsystem::MakePlaybackSound(UObject* Outer, USoundBase* Source, bool Loop)
{
    USoundCue* Cue = nullptr;
    if (auto* Wave = Cast<USoundWave>(Source))
    {
        Cue = NewObject<USoundCue>(Outer);
        Cue->VolumeMultiplier = 1.f;
        Cue->SoundClassObject = Wave->SoundClassObject;
        auto* Player = NewObject<USoundNodeWavePlayer>(Cue);
        Player->SetSoundWave(Wave);
        Player->bLooping = Loop;
        Cue->FirstNode = Player;
    }
    else if (auto* SourceCue = Cast<USoundCue>(Source))
    {
        // Private graph: the event switch overrides asset looping without changing the asset.
        Cue = DuplicateObject<USoundCue>(SourceCue, Outer);
        TSet<USoundNode*> Visited;
        TFunction<USoundNode*(USoundNode*)> Normalize = [&](USoundNode* Node) -> USoundNode*
        {
            while (auto* LoopNode = Cast<USoundNodeLooping>(Node))
                Node = LoopNode->ChildNodes.Num() ? LoopNode->ChildNodes[0].Get() : nullptr;
            if (!Node || Visited.Contains(Node)) return Node;
            Visited.Add(Node);
            if (auto* Player = Cast<USoundNodeWavePlayer>(Node)) Player->bLooping = false;
            for (auto& Child : Node->ChildNodes) Child = Normalize(Child);
            return Node;
        };
        Cue->FirstNode = Normalize(Cue->FirstNode);
        if (Loop)
        {
            auto* Repeater = NewObject<USoundNodeLooping>(Cue);
            Repeater->bLoopIndefinitely = true;
            Repeater->ChildNodes.Add(Cue->FirstNode);
            Cue->FirstNode = Repeater;
        }
    }
    else
    {
        // Other procedural sound types own their playback/loop logic.
        if (Loop || Source->IsLooping())
        {
            UE_LOG(LogTemp, Warning, TEXT("Narrative audio loop override requires SoundWave or SoundCue: %s"), *Source->GetPathName());
            return nullptr;
        }
        return Source;
    }
    Cue->CacheAggregateValues();
    // CacheAggregateValues only updates Duration while GIsEditor; runtime cues need it in packaged builds too.
    Cue->Duration = Cue->FirstNode ? Cue->FirstNode->GetDuration() : 0.f;
    return Cue;
}

void UVTGNarrativeAudioSubsystem::Play(UObject* Owner, UNarrativeComponent* Narrative, USoundBase* Sound,
    FName ID, bool Loop, bool StopWithDialogue, float FadeIn, float Volume, float FadeOut)
{
    if (!Owner || !Sound || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    USoundBase* Playback = MakePlaybackSound(this, Sound, Loop);
    if (!Playback) return;
    // A named tag owns one playback per scope. Untagged plays replace only the same untagged source.
    for (int32 I = Sounds.Num()-1; I >= 0; --I)
        if (Sounds[I].Owner == Owner && (ID.IsNone() ? (Sounds[I].ID.IsNone() && Sounds[I].Source == Sound) : Sounds[I].ID == ID))
            StopAt(I, 0.f);
    auto* Audio = NewObject<UAudioComponent>(this);
    Audio->bAutoActivate = false;
    Audio->bAutoDestroy = false;
    Audio->bAllowSpatialization = false;
    Audio->bIsUISound = true;
    Audio->SetSound(Playback);
    Audio->SetVolumeMultiplier(FMath::Max(0.f, Volume));
    Audio->RegisterComponentWithWorld(GetWorld());
    Audio->OnAudioFinishedNative.AddUObject(this, &UVTGNarrativeAudioSubsystem::Finished);
    FVTGNarrativeSound Entry;
    Entry.Audio = Audio; Entry.Source = Sound; Entry.PlaybackSound = Playback;
    Entry.Owner = Owner; Entry.ID = ID;
    Entry.Dialogue = Narrative ? Narrative->GetCurrentDialogue() : nullptr;
    Entry.StopWithDialogue = StopWithDialogue; Entry.FadeOut = FMath::Max(0.f, FadeOut);
    Sounds.Add(Entry);
    if (Narrative && Entry.Dialogue.IsValid() && StopWithDialogue)
    {
        Narrative->OnDialogueFinished.AddUniqueDynamic(this, &UVTGNarrativeAudioSubsystem::DialogueFinished);
        Bindings.AddUnique(Narrative);
    }
    if (FadeIn > 0.f) Audio->FadeIn(FadeIn, 1.f);
    else Audio->Play();
}

void UVTGNarrativeAudioSubsystem::StopAt(int32 Index, float FadeOut)
{
    auto* Audio = Sounds[Index].Audio.Get();
    const bool OwnsAudio = Sounds[Index].OwnsAudio;
    // External code may reuse its component for a different sound. That is not the registered playback.
    if (!OwnsAudio && IsValid(Audio) && Audio->Sound != Sounds[Index].PlaybackSound)
    {
        Audio->OnAudioFinishedNative.RemoveAll(this);
        Sounds.RemoveAt(Index);
        return;
    }
    if (FadeOut > 0.f && IsValid(Audio) && Audio->IsPlaying())
    {
        Sounds[Index].Stopping = true;
        Audio->FadeOut(FadeOut, 0.f);
        return;
    }
    Sounds.RemoveAt(Index);
    if (IsValid(Audio))
    {
        Audio->OnAudioFinishedNative.RemoveAll(this);
        Audio->Stop();
        if (OwnsAudio) Audio->DestroyComponent();
    }
}

void UVTGNarrativeAudioSubsystem::Stop(UObject* Owner, USoundBase* Sound, FName ID, float FadeOut)
{
    // Tag has priority. Only an absent tag permits the source-asset fallback; both absent is a no-op.
    if (!Owner || (ID.IsNone() && !Sound)) return;
    for (int32 I = Sounds.Num()-1; I >= 0; --I)
        if (Sounds[I].Owner == Owner && (ID.IsNone() ? Sounds[I].Source == Sound : Sounds[I].ID == ID))
            StopAt(I, FMath::Max(0.f, FadeOut));
}

bool UVTGNarrativeAudioSubsystem::RegisterExternal(UAudioComponent* Audio, UNarrativeComponent* Narrative, FName ID, bool StopWithDialogue)
{
    if (!IsValid(Audio) || !IsValid(Narrative) || !Audio->Sound || ID.IsNone() ||
        Audio->GetWorld()!=GetWorld() || Narrative->GetWorld()!=GetWorld()) return false;
    // Do not adopt or transfer ownership of a component created by this subsystem.
    for (const auto& Entry : Sounds) if (Entry.Audio==Audio && Entry.OwnsAudio) return false;
    UnregisterExternal(Audio);
    Stop(Narrative, nullptr, ID, 0.f);
    FVTGNarrativeSound Entry;
    Entry.Audio=Audio; Entry.Source=Audio->Sound; Entry.PlaybackSound=Audio->Sound;
    Entry.Owner=Narrative; Entry.ID=ID; Entry.OwnsAudio=false;
    Entry.StopWithDialogue=StopWithDialogue;
    Entry.Dialogue=Narrative->GetCurrentDialogue();
    Sounds.Add(Entry);
    Audio->OnAudioFinishedNative.AddUObject(this,&UVTGNarrativeAudioSubsystem::Finished);
    if (StopWithDialogue && Entry.Dialogue.IsValid())
    {
        Narrative->OnDialogueFinished.AddUniqueDynamic(this,&UVTGNarrativeAudioSubsystem::DialogueFinished);
        Bindings.AddUnique(Narrative);
    }
    return true;
}

void UVTGNarrativeAudioSubsystem::UnregisterExternal(UAudioComponent* Audio)
{
    for (int32 I=Sounds.Num()-1;I>=0;--I)
        if (Sounds[I].Audio==Audio && !Sounds[I].OwnsAudio)
        {
            if (IsValid(Audio)) Audio->OnAudioFinishedNative.RemoveAll(this);
            Sounds.RemoveAt(I);
        }
}

void UVTGNarrativeAudioSubsystem::Finished(UAudioComponent* Component)
{
    for (int32 I = Sounds.Num()-1; I >= 0; --I)
        if (Sounds[I].Audio == Component) { StopAt(I, 0.f); break; }
}

void UVTGNarrativeAudioSubsystem::DialogueFinished(UDialogue* Dialogue, bool)
{
    for (int32 I = Sounds.Num()-1; I >= 0; --I)
        if (Sounds[I].StopWithDialogue && Sounds[I].Dialogue == Dialogue && !Sounds[I].Stopping)
            StopAt(I, Sounds[I].FadeOut);
}

void UVTGNarrativeAudioSubsystem::Deinitialize()
{
    for (auto& Component : Bindings)
        if (Component.IsValid()) Component->OnDialogueFinished.RemoveAll(this);
    Bindings.Empty();
    while (!Sounds.IsEmpty()) StopAt(Sounds.Num()-1, 0.f);
    Super::Deinitialize();
}

void UVTGNarrativeAudioLibrary::PlayIndependentSound(APawn* Pawn, UNarrativeComponent* NarrativeComponent,
    USoundBase* NewSound, FName PlaybackID, bool Loop, bool StopWhenDialogueFinished,
    float FadeInTime, float VolumeMultiplier, float StopFadeOutTime)
{
    UObject* Owner = NarrativeComponent ? static_cast<UObject*>(NarrativeComponent) : Pawn;
    if (Owner && Owner->GetWorld())
        if (auto* State = Owner->GetWorld()->GetSubsystem<UVTGNarrativeAudioSubsystem>())
            State->Play(Owner, NarrativeComponent, NewSound, PlaybackID, Loop, StopWhenDialogueFinished,
                FadeInTime, VolumeMultiplier, StopFadeOutTime);
}

void UVTGNarrativeAudioLibrary::StopIndependentSound(APawn* Pawn, UNarrativeComponent* NarrativeComponent,
    USoundBase* SoundToStop, FName PlaybackID, float FadeOutTime)
{
    UObject* Owner = NarrativeComponent ? static_cast<UObject*>(NarrativeComponent) : Pawn;
    if (Owner && Owner->GetWorld())
        if (auto* State = Owner->GetWorld()->GetSubsystem<UVTGNarrativeAudioSubsystem>())
            State->Stop(Owner, SoundToStop, PlaybackID, FadeOutTime);
}

bool UVTGNarrativeAudioLibrary::RegisterAudioForNarrativeStop(UAudioComponent* AudioComponent,
    UNarrativeComponent* NarrativeComponent, FName PlaybackID, bool StopWhenDialogueFinished)
{
    if (IsValid(AudioComponent) && AudioComponent->GetWorld())
        if (auto* State=AudioComponent->GetWorld()->GetSubsystem<UVTGNarrativeAudioSubsystem>())
            return State->RegisterExternal(AudioComponent,NarrativeComponent,PlaybackID,StopWhenDialogueFinished);
    return false;
}

void UVTGNarrativeAudioLibrary::UnregisterAudioForNarrativeStop(UAudioComponent* AudioComponent)
{
    if (IsValid(AudioComponent) && AudioComponent->GetWorld())
        if (auto* State=AudioComponent->GetWorld()->GetSubsystem<UVTGNarrativeAudioSubsystem>())
            State->UnregisterExternal(AudioComponent);
}
