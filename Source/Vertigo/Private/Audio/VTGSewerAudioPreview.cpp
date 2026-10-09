// Opt-in, editor-build-only smoke test; no asset or level writes.
#if WITH_EDITOR
#include "Audio/VTGPlayerAudioComponent.h"
#include "AudioDevice.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

static FAutoConsoleCommandWithWorld GVTGCheckSewerAudio(
    TEXT("VTG.CheckSewerAudio"), TEXT("Wait 8 seconds, check sewer ambience/mixer reverb, then EXIT this game instance."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::Game) return;
        FTimerHandle Timer;
        TWeakObjectPtr<UWorld> WeakWorld=World;
        World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([WeakWorld]()
        {
            auto* W=WeakWorld.Get(); if (!W) return;
            auto* Player=UGameplayStatics::GetPlayerPawn(W,0);
            auto* Audio=Player?Player->FindComponentByClass<UVTGPlayerAudioComponent>():nullptr;
            auto* Camera=UGameplayStatics::GetPlayerCameraManager(W,0);
            bool Good=Audio && Camera;
            if (Good)
            {
                const int32 Space=Audio->ProbeSpace(Camera->GetCameraLocation());
                int32 Playing=0;
                for (TObjectIterator<UAudioComponent> I; I; ++I)
                    if (I->GetWorld()==W && (I->Sound==Audio->WaterBed || I->Sound==Audio->PipeBed))
                    {
                        UE_LOG(LogTemp,Display,TEXT("AUDIO SMOKE bed=%s playing=%d volume=%.3f"),*GetNameSafe(I->Sound),I->IsPlaying(),I->VolumeMultiplier);
                        Playing+=I->IsPlaying();
                    }
                auto Device=W->GetAudioDevice();
                auto* Reverb=Device?Device->GetCurrentReverbEffect():nullptr;
                UE_LOG(LogTemp,Display,TEXT("AUDIO SMOKE map=%s space=%d reverb=%s listener=%s"),
                    *UGameplayStatics::GetCurrentLevelName(W,true),Space,*GetNameSafe(Reverb),*Camera->GetCameraLocation().ToString());
                Good=Playing==2 && (Space==0 || Reverb!=nullptr);
            }
            UE_LOG(LogTemp,Display,TEXT("AUDIO SMOKE %s"),Good?TEXT("PASS"):TEXT("FAIL"));
            FPlatformMisc::RequestExitWithStatus(false,Good?0:1);
        }),8.f,false);
    }));
#endif
