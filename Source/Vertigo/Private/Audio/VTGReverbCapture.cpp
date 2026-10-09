// Explicit editor-game diagnostic: capture one isolated footstep and its reverb tail, then exit.
#if WITH_EDITOR
#include "ActiveSound.h"
#include "Audio.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioThread.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
static void Later(UWorld *W, float Seconds, TFunction<void()> F)
{
    FTimerHandle H;
    W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda(MoveTemp(F)), Seconds, false);
}
static FAutoConsoleCommandWithWorldAndArgs CaptureReverb(
    TEXT("VTG.CaptureReverb"),
    TEXT("Capture isolated footstep; optional dry argument disables reverb send. Exits GAME after capture."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args, UWorld *W)
        {
            if (!W || W->WorldType != EWorldType::Game)
                return;
            FApp::SetUnfocusedVolumeMultiplier(1.f);
            FApp::SetVolumeMultiplier(1.f);
            const bool Dry = Args.Contains(TEXT("dry"));
            Later(W, 7,
                  [W, Dry]()
                  {
                      auto *P = UGameplayStatics::GetPlayerPawn(W, 0);
                      auto *C = P ? P->FindComponentByClass<UVTGPlayerAudioComponent>() : nullptr;
                      if (!C || !W->GetAudioDevice())
                      {
                          FPlatformMisc::RequestExitWithStatus(false, 1);
                          return;
                      }
                      C->SetComponentTickEnabled(false);
                      auto Device=W->GetAudioDevice();
                      FAudioThread::RunCommandOnAudioThread([Device]()
                      {
                          for (auto* S:Device->GetActiveSounds()) for (const auto& V:S->GetWaveInstances())
                              UE_LOG(LogTemp,Display,TEXT("REVERB AMBIENCE %s enabled=%d send=%.3f"),
                                     *GetNameSafe(V.Value->WaveData),V.Value->bReverb,V.Value->ReverbSendLevel);
                      });
                      W->GetAudioDevice()->StopAllSounds(true);
                      Later(W, 4,
                            [W, C, Dry]()
                            {
                                UAudioMixerBlueprintLibrary::StartRecordingOutput(W, 5);
                                Later(W, .5f,
                                      [W, C, Dry]()
                                      {
                                          auto *Att = NewObject<USoundAttenuation>(C);
                                          Att->Attenuation = C->StepAttenuation->Attenuation;
                                          if (Dry)
                                              Att->Attenuation.ManualReverbSendLevel = 0;
                                          auto *Sound = C->ConcreteSteps[0].Get();
                                          auto *Class = Sound->GetSoundClass();
                                          UE_LOG(LogTemp, Display,
                                                 TEXT("REVERB CAPTURE soundClass=%s classReverb=%d 2Dsend=%.3f "
                                                      "configured3Dsend=%.3f"),
                                                 *GetNameSafe(Class), Class ? Class->Properties.bReverb : 0,
                                                 Class ? Class->Properties.Default2DReverbSendAmount : 0,
                                                 Att->Attenuation.ManualReverbSendLevel);
                                          UGameplayStatics::PlaySoundAtLocation(
                                              W, Sound,
                                              UGameplayStatics::GetPlayerCameraManager(W, 0)->GetCameraLocation(),
                                              FRotator::ZeroRotator, .6f, 1, 0, Att);
                                          Later(W, .08f,
                                                [W]()
                                                {
                                                    auto Device = W->GetAudioDevice();
                                                    FAudioThread::RunCommandOnAudioThread(
                                                        [Device]()
                                                        {
                                                            for (auto *S : Device->GetActiveSounds())
                                                                for (const auto &V : S->GetWaveInstances())
                                                                    UE_LOG(LogTemp, Display,
                                                                           TEXT("REVERB VOICE %s enabled=%d send=%.3f"),
                                                                           *GetNameSafe(V.Value->WaveData),
                                                                           V.Value->bReverb, V.Value->ReverbSendLevel);
                                                        });
                                                });
                                      });
                                Later(W, 4.5f,
                                      [W, Dry]()
                                      {
                                          UAudioMixerBlueprintLibrary::StopRecordingOutput(
                                              W, EAudioRecordingExportType::WavFile,
                                              Dry ? TEXT("ReverbDry") : TEXT("ReverbWet"),
                                              FPaths::ProjectSavedDir() / TEXT("BouncedWavFiles"));
                                          Later(W, 2, []() { FPlatformMisc::RequestExitWithStatus(false, 0); });
                                      });
                            });
                  });
        }));
#endif
