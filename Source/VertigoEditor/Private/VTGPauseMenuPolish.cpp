#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"

// Authoring utility: preserve the original buttons, graph bindings and texts.
int32 VTGPolishPauseMenu(const FString& Params)
{
 const bool Verify=FParse::Param(*Params,TEXT("Verify"));
 const FString MainFile=FPaths::ProjectContentDir()/TEXT("ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu.uasset");
 const FString PauseFile=FPaths::ProjectContentDir()/TEXT("ThirdPerson/Blueprints/UI/WBP_PauseMenu.uasset");
 const auto MainHash=FMD5Hash::HashFile(*MainFile);
 auto* Main=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu"));
 auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu"));check(Main && BP && BP->WidgetTree);
 auto* Ref=CastChecked<UButton>(Main->WidgetTree->FindWidget(TEXT("Button")));
 auto* RefLabel=CastChecked<UTextBlock>(Ref->GetContent());
 auto* RefSlot=CastChecked<UButtonSlot>(RefLabel->Slot);
 auto* Canvas=CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
 auto* Blur=CastChecked<UBackgroundBlur>(BP->WidgetTree->FindWidget(TEXT("BackgroundBlur_90")));
 auto* Shade=CastChecked<UImage>(BP->WidgetTree->FindWidget(TEXT("Image_68")));
 auto* Buttons=CastChecked<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("VerticalBox_137")));
 auto* Title=CastChecked<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("TextBlock_81")));
 auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/2DArt/UI/GameOption/Pause_copy"));check(Texture);
 auto* Art=Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("PauseArtwork")));
 auto Fullscreen=[](UWidget* Widget,int32 Z){auto* S=CastChecked<UCanvasPanelSlot>(Widget->Slot);S->SetAnchors(FAnchors(0,0,1,1));S->SetOffsets(FMargin(0));S->SetZOrder(Z);Widget->SetVisibility(ESlateVisibility::HitTestInvisible);};
 if(!Verify)
 {
  const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/PauseMenuPolish/Backup/WBP_PauseMenu.uasset");
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
  if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*PauseFile,false)==COPY_OK);
  BP->Modify();
  Blur->SetBlurStrength(6.f);Blur->SetRenderOpacity(1.f);Blur->SetApplyAlphaToBlur(false);
  // A fallback should not become an opaque black screen on low quality settings.
  FSlateBrush Fallback;Fallback.TintColor=FLinearColor::Transparent;Fallback.DrawAs=ESlateBrushDrawType::NoDrawType;
  Blur->SetLowQualityFallbackBrush(Fallback);
  Fullscreen(Blur,0);
  Shade->SetColorAndOpacity(FLinearColor(0,0,0,.5f));Shade->SetRenderOpacity(1.f);Fullscreen(Shade,1);
  if(!Art){Art=BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("PauseArtwork"));Canvas->AddChildToCanvas(Art);}
  Art->SetBrushFromTexture(Texture);Art->SetColorAndOpacity(FLinearColor::White);Fullscreen(Art,2);
  CastChecked<UCanvasPanelSlot>(Title->Slot)->SetZOrder(3);
  // Match the reference button width; leave the menu centered and its title in place.
  auto* Layout=CastChecked<UCanvasPanelSlot>(Buttons->Slot);Layout->SetZOrder(4);
  Layout->SetAnchors(FAnchors(.5f,.5f));Layout->SetAlignment(FVector2D(.5f,0));Layout->SetPosition(FVector2D(0,-172.54f));Layout->SetSize(FVector2D(345.38f,500));
  for(const TCHAR* Name:{TEXT("B_Continue"),TEXT("B_Exit")})
  {
   auto* B=CastChecked<UButton>(BP->WidgetTree->FindWidget(Name));auto* L=CastChecked<UTextBlock>(B->GetContent());
   B->SetStyle(Ref->GetStyle());B->SetColorAndOpacity(Ref->GetColorAndOpacity());B->SetBackgroundColor(Ref->GetBackgroundColor());
   L->SetFont(RefLabel->GetFont());L->SetColorAndOpacity(RefLabel->GetColorAndOpacity());L->SetShadowColorAndOpacity(RefLabel->GetShadowColorAndOpacity());L->SetShadowOffset(RefLabel->GetShadowOffset());L->SetJustification(ETextJustify::Left);
   auto* S=CastChecked<UButtonSlot>(L->Slot);S->SetPadding(RefSlot->GetPadding());S->SetHorizontalAlignment(RefSlot->GetHorizontalAlignment());S->SetVerticalAlignment(RefSlot->GetVerticalAlignment());
   auto* V=CastChecked<UVerticalBoxSlot>(B->Slot);V->SetPadding(FMargin(0,80,0,-15));V->SetHorizontalAlignment(HAlign_Fill);
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(Log.NumErrors==0);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*PauseFile,Args));
 }
 check(Art && Art->GetBrush().GetResourceObject()==Texture && FMath::IsNearlyEqual(Art->GetColorAndOpacity().A,1.f));
 check(FMath::IsNearlyEqual(Shade->GetColorAndOpacity().A,.5f) && FMath::IsNearlyEqual(Blur->GetBlurStrength(),6.f));
 check(CastChecked<UCanvasPanelSlot>(Blur->Slot)->GetZOrder()==0 && CastChecked<UCanvasPanelSlot>(Shade->Slot)->GetZOrder()==1 && CastChecked<UCanvasPanelSlot>(Art->Slot)->GetZOrder()==2 && CastChecked<UCanvasPanelSlot>(Buttons->Slot)->GetZOrder()==4);
 for(const TCHAR* Name:{TEXT("B_Continue"),TEXT("B_Exit")})
 {
  auto* B=CastChecked<UButton>(BP->WidgetTree->FindWidget(Name));auto* L=CastChecked<UTextBlock>(B->GetContent());
  check(B->GetStyle().Normal.GetResourceObject()==Ref->GetStyle().Normal.GetResourceObject() && B->GetStyle().Hovered.GetResourceObject()==Ref->GetStyle().Hovered.GetResourceObject() && B->GetStyle().Pressed.GetResourceObject()==Ref->GetStyle().Pressed.GetResourceObject());
  check(L->GetFont()==RefLabel->GetFont() && L->GetColorAndOpacity()==RefLabel->GetColorAndOpacity());
 }
 // Hash the on-disk reference too: loading it must never save or modify Main Menu.
 check(MainHash==FMD5Hash::HashFile(*MainFile));
 FString Preview;
 if(FParse::Value(*Params,TEXT("Preview="),Preview))
 {
  if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
  auto* Widget=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);check(Widget->Initialize());
  // A deliberately simple colored backdrop makes the retained background visible in this UI-only preview.
  auto Root=SNew(SOverlay)
   +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.12,.20,.24,1))]
   +SOverlay::Slot().HAlign(HAlign_Left).Padding(80,140,0,140)[SNew(SBorder).Padding(FMargin(160,0)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.25,.12,.055,1))]
   +SOverlay::Slot()[Widget->TakeWidget()];
  if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
  FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(Root,FVector2D(1920,1080));FlushRenderingCommands();
  for(int I=0;I<3;++I){Renderer.DrawWidget(Target,Root,FVector2D(1920,1080),.016f);FlushRenderingCommands();}
  TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*Preview));
 }
 UE_LOG(LogTemp,Display,TEXT("PAUSE_MENU_PASS: button normal/hover/pressed artwork, font/color/padding, blur 6, black 50%%, artwork original alpha, layer order and unchanged MainMenu verified"));
 return 0;
}
