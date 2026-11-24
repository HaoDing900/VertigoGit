#include "Audio/VTGCombatMusic.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

bool UVTGCombatMusicSubsystem::FindRow(const UObject* WorldContext, FName Track, FVTGCombatMusicRow& OutRow)
{
	if (Track.IsNone())
	{
		return false;
	}
	const auto* Settings = GetDefault<UVTGCombatMusicSettings>();
	UDataTable* Table = Settings ? Settings->Tracks.LoadSynchronous() : nullptr;
	if (!Table)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGCombatMusic] No track table configured (Project Settings -> Vertigo -> VTG Combat Music)."));
		return false;
	}
	const auto* Row = Table->FindRow<FVTGCombatMusicRow>(Track, TEXT("VTGCombatMusic"), /*bWarnIfMissing=*/false);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGCombatMusic] %s has no row \"%s\"."), *Table->GetName(), *Track.ToString());
		return false;
	}
	OutRow = *Row;
	return true;
}

void UVTGCombatMusicSubsystem::Start(FName Track)
{
	if (Track == Playing && Music)
	{
		return;
	}

	FVTGCombatMusicRow Row;
	USoundBase* Sound = FindRow(this, Track, Row) ? Row.Music.LoadSynchronous() : nullptr;
	if (!Sound)
	{
		// Whatever the last fight left playing has to go either way: a missing row must not leave the
		// previous encounter's music over this one.
		Stop();
		return;
	}

	Stop(0.f);
	// bAutoDestroy=false so the component survives the end of the wave and HandleFinished can
	// restart it; that is what makes a plain non-looping SoundWave loop for the whole fight.
	Music = UGameplayStatics::CreateSound2D(this, Sound, Row.Volume, 1.f, 0.f, nullptr, /*bPersistAcrossLevelTransition=*/false,
		/*bAutoDestroy=*/false);
	if (!Music)
	{
		Playing = NAME_None;
		return;
	}
	Music->OnAudioFinished.AddDynamic(this, &UVTGCombatMusicSubsystem::HandleFinished);
	Playing = Track;
	ActiveFadeOut = Row.FadeOut;
	Music->FadeIn(Row.FadeIn, Row.Volume);
	UE_LOG(LogTemp, Display, TEXT("[VTGCombatMusic] start %s (%s) volume=%.2f"), *Track.ToString(), *Sound->GetName(), Row.Volume);
}

void UVTGCombatMusicSubsystem::Stop(float FadeOutOverride)
{
	if (!Music)
	{
		Playing = NAME_None;
		return;
	}
	const float Fade = FadeOutOverride >= 0.f ? FadeOutOverride : ActiveFadeOut;
	// Drop the restart hook first, or the fade's own finish would start the track over.
	Music->OnAudioFinished.RemoveAll(this);
	Playing = NAME_None;
	if (Fade > 0.f)
	{
		Music->bAutoDestroy = true;
		Music->FadeOut(Fade, 0.f);
	}
	else
	{
		Music->Stop();
	}
	Music = nullptr;
}

void UVTGCombatMusicSubsystem::Deinitialize()
{
	Stop(0.f);
	Super::Deinitialize();
}

void UVTGCombatMusicSubsystem::HandleFinished()
{
	if (!Playing.IsNone() && Music)
	{
		Music->Play();
	}
}

namespace VTGCombatMusic
{
UVTGCombatMusicSubsystem* Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UVTGCombatMusicSubsystem>() : nullptr;
}
}

void UVTGCombatMusicLibrary::StartCombatMusic(const UObject* WorldContextObject, FName Track)
{
	if (auto* Subsystem = VTGCombatMusic::Get(WorldContextObject))
	{
		Subsystem->Start(Track);
	}
}

void UVTGCombatMusicLibrary::StopCombatMusic(const UObject* WorldContextObject, float FadeOutOverride)
{
	if (auto* Subsystem = VTGCombatMusic::Get(WorldContextObject))
	{
		Subsystem->Stop(FadeOutOverride);
	}
}

FName UVTGCombatMusicLibrary::GetCombatMusicTrack(const UObject* WorldContextObject)
{
	auto* Subsystem = VTGCombatMusic::Get(WorldContextObject);
	return Subsystem ? Subsystem->CurrentTrack() : NAME_None;
}
