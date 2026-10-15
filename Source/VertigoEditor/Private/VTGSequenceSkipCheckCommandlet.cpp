#include "VTGSequenceSkipCheckCommandlet.h"
#include "VTGCinematicRegressionFixture.h"
#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "UI/VTGSequenceSkipSubsystem.h"
#include "UI/VTGHintLayer.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LevelSequence.h"

UVTGSequenceSkipCheckCommandlet::UVTGSequenceSkipCheckCommandlet()
{ IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true; }
int32 UVTGSequenceSkipCheckCommandlet::Main(const FString& Params)
{
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    if(!VTGRunSequenceSkipChecks())return 1;
    if(FParse::Param(*Params,TEXT("Audit")))
    {
        auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);FARFilter Filter;Filter.PackagePaths.Add(TEXT("/Game/LevelNSequence"));Filter.bRecursivePaths=true;Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());TArray<FAssetData> Assets;Registry.GetAssets(Filter,Assets);int Allowed=0;for(const auto& Asset:Assets)if(auto* Sequence=Cast<ULevelSequence>(Asset.GetAsset());UVTGSequenceSkipSubsystem::IsSkipAllowed(Sequence)){++Allowed;UE_LOG(LogTemp,Display,TEXT("SKIP ENABLED: %s"),*Asset.PackageName.ToString());}UE_LOG(LogTemp,Display,TEXT("SKIP AUDIT: %d / %d story sequence assets opted in"),Allowed,Assets.Num());
    }
    FString Output;if(FParse::Value(*Params,TEXT("Preview="),Output))
    {
        auto* Class=LoadClass<UVTGHintLayer>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_HintLayer.WBP_HintLayer_C"));check(Class);auto* Layer=NewObject<UVTGHintLayer>(GetTransientPackage(),Class);Layer->Initialize();FSlateBrush Brush;Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/T_Space_Key_Dark")));Layer->UpdateSkipHint(true,.5f,Brush);
        auto Root=SNew(SOverlay)+SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025,.035,.045,1))]+SOverlay::Slot()[Layer->TakeWidget()];FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(Root,FVector2D(1920,1080));FlushRenderingCommands();TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*Output));
    }
    return 0;
}
