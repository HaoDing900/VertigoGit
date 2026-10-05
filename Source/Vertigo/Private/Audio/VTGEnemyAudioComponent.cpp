#include "Audio/VTGEnemyAudioComponent.h"

#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UVTGEnemyAudioComponent::UVTGEnemyAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // fight state changes slowly; no need to poll every frame
}

void UVTGEnemyAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	InCombatProp = Source.Find(GetOwner(), HealthProperty, MaxHealthProperty)
		? FindFProperty<FBoolProperty>(Source.Get()->GetClass(), InCombatProperty)
		: nullptr;
	if (!Source.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGEnemyAudio] %s: no component exposes %s / %s - no music or death sound."),
			*GetNameSafe(GetOwner()), *HealthProperty.ToString(), *MaxHealthProperty.ToString());
		SetComponentTickEnabled(false);
	}
}

void UVTGEnemyAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Music)
	{
		Music->Stop();
		Music = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UVTGEnemyAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Source.IsValid())
	{
		return;
	}

	const bool bDead = Source.GetHealth() <= 0.f;
	if (bDead && !bDied && DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetOwner()->GetActorLocation());
	}
	bDied = bDead;
	DeadFor = bDead ? DeadFor + DeltaTime : 0.f;

	if (CombatMusic)
	{
		// Death also clears InCombat, so check health first: a kill keeps the music up a moment.
		const bool bInCombat = InCombatProp && InCombatProp->GetPropertyValue_InContainer(Source.Get());
		SetMusicPlaying(bDead ? DeadFor < MusicHoldAfterDeath : bInCombat);
	}
}

void UVTGEnemyAudioComponent::SetMusicPlaying(bool bPlay)
{
	if (bPlay == bMusicWanted)
	{
		return;
	}
	bMusicWanted = bPlay;

	if (bPlay)
	{
		if (!Music)
		{
			Music = UGameplayStatics::CreateSound2D(this, CombatMusic, MusicVolume, 1.f, 0.f, nullptr, false, /*bAutoDestroy=*/false);
			if (!Music)
			{
				return;
			}
			Music->OnAudioFinished.AddDynamic(this, &UVTGEnemyAudioComponent::HandleMusicFinished);
		}
		Music->FadeIn(MusicFadeInTime, MusicVolume);
	}
	else if (Music)
	{
		Music->FadeOut(MusicFadeOutTime, 0.f);
	}
}

void UVTGEnemyAudioComponent::HandleMusicFinished()
{
	if (bMusicWanted && Music)
	{
		Music->Play();
	}
}
