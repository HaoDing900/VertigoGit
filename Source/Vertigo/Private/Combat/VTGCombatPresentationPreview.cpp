// Editor-build-only visual check. Invoked explicitly; never changes a level or saved game.
#if WITH_EDITOR
#include "UI/VTGBossHealthBar.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

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
        Bar->AddToViewport(20);
    }));
#endif
