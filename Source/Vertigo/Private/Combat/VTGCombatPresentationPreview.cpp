// Editor-build-only visual check. Invoked explicitly; never changes a level or saved game.
#if WITH_EDITOR
#include "UI/VTGBossHealthBar.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

static FAutoConsoleCommandWithWorld GVTGPreviewCombatHUD(
    TEXT("VTG.PreviewCombatHUD"), TEXT("Show the real boss widget at 68 percent health for visual review."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        static TWeakObjectPtr<UVTGBossHealthBar> Previous;
        auto* PC = UGameplayStatics::GetPlayerController(World, 0);
        if (!PC) return;
        auto* Class = LoadClass<UVTGBossHealthBar>(nullptr, TEXT("/Game/Widget/BossHealthBar/WBP_BossHealthBar.WBP_BossHealthBar_C"));
        if (!Class) return;
        if (Previous.IsValid()) Previous->RemoveFromParent();
        auto* Bar = CreateWidget<UVTGBossHealthBar>(PC, Class);
        if (!Bar) return;
        Previous = Bar;
        Bar->SetBossName(FText::FromString(TEXT("目孵巢 (Eyed Broodnest)")));
        Bar->SetHealth(1000, 1000);
        Bar->SetHealth(680, 1000);
        Bar->SetShown(true);
        Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
        Bar->SetRenderOpacity(1.f);
        Bar->SetCausticOverlay(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/FX/M_BossHealth_CausticHUD")),.65f);
        Bar->AddToViewport(FParse::Param(FCommandLine::Get(),TEXT("VTGBossHUDScreenshot"))?100000:20);
        if(FParse::Param(FCommandLine::Get(),TEXT("VTGBossHUDScreenshot")))
        {
            const double Started=FPlatformTime::Seconds();
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Started,Requested=false,Refreshed=false](float)mutable
            {
                const double Age=FPlatformTime::Seconds()-Started;
                // Loading-screen startup can replace the viewport contents; re-add after it settles.
                if(!Refreshed && Age>=16){Refreshed=true;if(GEngine && GEngine->GameViewport)IConsoleManager::Get().ProcessUserConsoleInput(TEXT("VTG.PreviewCombatHUD"),*GLog,GEngine->GameViewport->GetWorld());}
                if(!Requested && Age>=20){Requested=true;FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Scripts/SewerBossSync/live-hud.png"),true,false);}
                if(Age>=23){FPlatformMisc::RequestExit(false);return false;}return true;
            }));
        }
    }));
#endif
