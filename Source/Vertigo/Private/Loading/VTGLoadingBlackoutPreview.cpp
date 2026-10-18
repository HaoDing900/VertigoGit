// Explicit editor-build-only travel probe; never runs during normal play.
#if WITH_EDITOR
#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Loading/VTGMediaLoadingPageSettings.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"

static FAutoConsoleCommandWithWorld GVTGVerifyLoadingBlackout(
    TEXT("VTG.VerifyLoadingBlackout"),TEXT("Test loading travel in an isolated -game process, then exit that process."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        // Require an explicit command-line opt-in so typing the command in a work session cannot exit it.
        if (!FParse::Param(FCommandLine::Get(),TEXT("VTGLoadingProbe")) || !World || !World->GetGameInstance()) return;
        struct FProbe
        {
            TWeakObjectPtr<UVTGMediaLoadingPageSystem> System;
            TWeakObjectPtr<UWorld> Original;
            double Started=0;
            double WaitingSince=0;
            bool Shown=false;
        };
        auto Probe=MakeShared<FProbe>();Probe->Original=World;Probe->WaitingSince=FPlatformTime::Seconds();
        Probe->System=World->GetGameInstance()->GetSubsystem<UVTGMediaLoadingPageSystem>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float)
        {
            if (Probe->System.IsValid() && Probe->Started==0 && FPlatformTime::Seconds()-Probe->WaitingSince<120)
            {
                // Initial map startup has its own automatic loading page. Wait for it before requesting another travel.
                if (Probe->System->IsLoadingPageVisible()) return true;
                Probe->Original=Probe->System->GetGameInstance()->GetWorld();
                Probe->Started=FPlatformTime::Seconds();
                Probe->System->OpenLevelWithLoadingPage(TEXT("/Engine/Maps/Entry"),true,TEXT("game=/Script/Engine.GameModeBase"));
                Probe->Shown=Probe->System->IsLoadingPageVisible();
                UE_LOG(LogTemp,Display,TEXT("BLACKOUT TRAVEL START: overlay visible before travel=%d"),Probe->Shown);
                return true;
            }
            const double Elapsed=FPlatformTime::Seconds()-Probe->Started;
            if (!Probe->System.IsValid() || Elapsed>60)
            {
                UE_LOG(LogTemp,Error,TEXT("BLACKOUT TRAVEL FAIL: timeout or lost subsystem after %.2fs"),Elapsed);
                FPlatformMisc::RequestExitWithStatus(false,1);return false;
            }
            if (Probe->System->IsLoadingPageVisible()) return true;
            auto* NewWorld=Probe->System->GetGameInstance()->GetWorld();
            const auto* Settings=GetDefault<UVTGMediaLoadingPageSettings>();
            const double Minimum=Settings->InitialBlackHoldDuration+Settings->FadeInDuration+Settings->MinimumDisplayTime+Settings->FadeToBlackDuration+Settings->HoldAfterLoadDuration+Settings->FadeOutDuration;
            const bool OK=Probe->Shown && NewWorld && NewWorld!=Probe->Original.Get() && NewWorld->HasBegunPlay() && Elapsed+.1>=Minimum;
            if(OK) { UE_LOG(LogTemp,Display,TEXT("BLACKOUT TRAVEL PASS: world replaced, begun play, overlay removed; elapsed %.2fs, minimum %.2fs"),Elapsed,Minimum); }
            else { UE_LOG(LogTemp,Error,TEXT("BLACKOUT TRAVEL FAIL: final world or timing; elapsed %.2fs, minimum %.2fs"),Elapsed,Minimum); }
            FPlatformMisc::RequestExitWithStatus(false,OK?0:1);return false;
        }),.1f);
    }));
#endif
