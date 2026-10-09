#include "Blueprint/WidgetTree.h"
#include "Combat/VTGCombatPresentation.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Components/ProgressBar.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/VTGBossHealthBar.h"
#include "WidgetBlueprint.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"

int32 VTGRenderCombatHUD(const FString &Output)
{
    if (!FSlateApplication::IsInitialized())
        FSlateApplication::InitializeAsStandaloneApplication(
            FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer"))
                .CreateSlateRHIRenderer());
    auto *BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Widget/BossHealthBar/WBP_BossHealthBar"));
    check(BP);
    auto *Bar = NewObject<UVTGBossHealthBar>(GetTransientPackage(), BP->GeneratedClass);
    Bar->Initialize();
    Bar->SetBossName(FText::FromString(TEXT("目孵巢 (Eyed Broodnest)")));
    Bar->SetHealth(1000, 1000);
    Bar->SetHealth(680, 1000);
    Bar->SetShown(true);
    Bar->SetRenderOpacity(1);
    auto *Hearts = NewObject<UVTGPlayerHearts>();
    Hearts->Initialize();
    Hearts->Full = LoadObject<UTexture2D>(nullptr, TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Filled"));
    Hearts->Empty = LoadObject<UTexture2D>(nullptr, TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Empty"));
    Hearts->Health = 68;
    Hearts->MaxHealth = 100;
    auto Root = SNew(SOverlay) +
                SOverlay::Slot()[SNew(SBorder)
                                     .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                     .BorderBackgroundColor(FLinearColor(.025, .04, .055, 1))] +
                SOverlay::Slot()[Bar->TakeWidget()] + SOverlay::Slot()[Hearts->TakeWidget()];
    if (GShaderCompilingManager)
        GShaderCompilingManager->FinishAllCompilation();
    auto* HP=CastChecked<UProgressBar>(Bar->WidgetTree->FindWidget(TEXT("HealthBar")));
    auto* M=Cast<UMaterial>(HP->GetWidgetStyle().FillImage.GetResourceObject());
    UE_LOG(LogTemp,Display,TEXT("UI DEBUG percent=%f tint=%s material=%s opacityExpr=%s emissiveExpr=%s hasUObject=%d"),HP->GetPercent(),*HP->GetFillColorAndOpacity().ToString(),*GetNameSafe(M),M?*GetNameSafe(M->GetEditorOnlyData()->Opacity.Expression):TEXT("none"),M?*GetNameSafe(M->GetEditorOnlyData()->EmissiveColor.Expression):TEXT("none"),HP->GetWidgetStyle().FillImage.HasUObject());
    if(M){auto* MR=M->GetMaterialResource(ERHIFeatureLevel::SM6);UE_LOG(LogTemp,Display,TEXT("UI DEBUG material resource=%p shadermap=%p"),MR,MR?MR->GetGameThreadShaderMap():nullptr);if(MR)for(const auto& E:MR->GetCompileErrors())UE_LOG(LogTemp,Error,TEXT("UI shader: %s"),*E);}
    if(FParse::Param(FCommandLine::Get(),TEXT("Plain"))){auto Style=HP->GetWidgetStyle();Style.FillImage=*FCoreStyle::Get().GetBrush("WhiteBrush");HP->SetWidgetStyle(Style);HP->SetFillColorAndOpacity(FLinearColor(.5,.02,.03));}
    // Slate UI materials require scene texture uniform buffers, even for a widget-only preview.
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).SetTransactional(false);
    auto* PreviewWorld=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CombatHUDPreview"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Capture=PreviewWorld->SpawnActor<ASceneCapture2D>();
    auto* SceneRT=NewObject<UTextureRenderTarget2D>();SceneRT->InitAutoFormat(64,64);SceneRT->UpdateResourceImmediate();
    Capture->GetCaptureComponent2D()->TextureTarget=SceneRT;Capture->GetCaptureComponent2D()->CaptureSource=SCS_FinalColorLDR;Capture->GetCaptureComponent2D()->bCaptureEveryFrame=false;Capture->GetCaptureComponent2D()->CaptureScene();FlushRenderingCommands();
    FWidgetRenderer Renderer(true);
    auto *RT = Renderer.DrawWidget(Root, FVector2D(1280, 720));
    FlushRenderingCommands();
    check(RT);
    for(int32 Frame=0;Frame<3;++Frame){ if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();Renderer.DrawWidget(RT,Root,FVector2D(1280,720),.016f);FlushRenderingCommands(); }
    TArray<FColor> Pixels;
    RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
    TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(1280, 720, Pixels, PNG);
    const bool OK = FFileHelper::SaveArrayToFile(PNG, *Output);
    UE_LOG(LogTemp, Display, TEXT("HUD preview %s: %s"), OK ? TEXT("saved") : TEXT("failed"), *Output);
    PreviewWorld->DestroyWorld(false);
    return OK ? 0 : 4;
}
