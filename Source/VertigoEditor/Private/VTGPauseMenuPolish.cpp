#include "WidgetBlueprint.h"
#include "UI/VTGPauseMenu.h"
#include "UI/VTGLanguageSettingsPage.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "VTGManualLocalization.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "UObject/UObjectHash.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
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
 if(FParse::Param(*Params,TEXT("LanguageButton")))
 {
  if(!Verify)
  {
   check(BP->ParentClass==UUserWidget::StaticClass() || BP->ParentClass==UVTGPauseMenu::StaticClass());
   const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/PauseLanguage/WBP_PauseMenu.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*PauseFile,false)==COPY_OK);
   BP->ParentClass=UVTGPauseMenu::StaticClass();FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);
   FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*PauseFile,Args));
  }
  check(BP->GeneratedClass->IsChildOf(UVTGPauseMenu::StaticClass()));
  if(FParse::Param(*Params,TEXT("LanguageTest")))
  {
   FEditorScriptExecutionGuard Guard;
   if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
   FString SavedLanguage;const bool HadLanguage=GConfig->GetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),SavedLanguage,GGameUserSettingsIni);
   auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
   auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("PauseLanguageTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
   auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* Dummy=GI->GetWorld();GI->GetWorldContext()->SetCurrentWorld(W);W->SetGameInstance(GI);Dummy->DestroyWorld(false);
   auto* PC=W->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));PC->SetPlayer(Local);
   auto* Widget=CreateWidget<UUserWidget>(PC,BP->GeneratedClass.Get());check(Widget);auto Slate=Widget->TakeWidget();
   auto* Button=CastChecked<UButton>(Widget->GetWidgetFromName(TEXT("B_Language")));auto* Label=CastChecked<UTextBlock>(Button->GetContent());auto* Ref=CastChecked<UButton>(Widget->GetWidgetFromName(TEXT("B_Continue")));auto* List=CastChecked<UVerticalBox>(Widget->GetWidgetFromName(TEXT("VerticalBox_137")));
   check(List->GetChildIndex(Button)==List->GetChildIndex(Ref)+1);
   check(Button->GetStyle().Normal.GetResourceObject()==Ref->GetStyle().Normal.GetResourceObject());check(Label->GetFont()==CastChecked<UTextBlock>(Ref->GetContent())->GetFont());
   auto* Languages=GI->GetSubsystem<UVTGLocalizationSubsystem>();check(Languages && Languages->SetLanguage(TEXT("origin")));
   // No world tick is advanced during language switching.
   Button->OnClicked.Broadcast();check(Languages->GetLanguage()==TEXT("origin") && Label->GetText().ToString()==TEXT("Language"));
   UVTGLanguageSettingsPage* Page=nullptr;
   Widget->WidgetTree->ForEachWidget([&](UWidget* Child){if(auto* P=Cast<UVTGLanguageSettingsPage>(Child))Page=P;});check(Page);
   auto* Selection=CastChecked<UVTGLanguageComboBox>(Page->GetWidgetFromName(TEXT("LanguageSelection")));
   Selection->SetSelectedOption(TEXT("简体中文"));check(Languages->GetLanguage()==TEXT("zh-Hans"));
   Selection->SetSelectedOption(TEXT("English"));check(Languages->GetLanguage()==TEXT("en-US"));
#if WITH_EDITOR
   Selection->SetSelectedOption(TEXT("Origin (Dev)"));check(Languages->GetLanguage()==TEXT("origin"));
   Selection->SetSelectedOption(TEXT("简体中文"));check(Languages->GetLanguage()==TEXT("zh-Hans"));
#endif
   auto* MapBP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/MapWBP/WBP_Map_1"));check(MapBP);
   auto* Map=CreateWidget<UUserWidget>(PC,MapBP->GeneratedClass.Get());check(Map);Map->TakeWidget();
   auto Read=[&](const TCHAR* Node,const TCHAR* Field){FText Value;auto* O=Map->GetWidgetFromName(Node);check(O);Value=FindFProperty<FTextProperty>(O->GetClass(),Field)->GetPropertyValue_InContainer(O);return Value;};
   const FText South=Read(TEXT("WBP_MapNode_3"),TEXT("LocationDisplayName"));
   const FText Description=Read(TEXT("WBP_MapNode_2"),TEXT("LocationDescription"));
   for(int32 Cycle=0;Cycle<3;++Cycle)
   {
    check(Languages->SetLanguage(TEXT("zh-Hans")));check(South.ToString()==TEXT("南部隧道"));
    check(Languages->SetLanguage(TEXT("en-US")));check(South.ToString()==TEXT("South Tunnel"));check(Description.ToString().StartsWith(TEXT("The Sump Stream")));
    check(Languages->SetLanguage(TEXT("origin")));check(Description.ToString().StartsWith(TEXT("下游区主要")));
   }
   UE_LOG(LogTemp,Display,TEXT("MAP_LOCALIZATION_SWITCH_PASS: actual saved South name and Commerce description zh/en/origin repeated three times"));
   check(Languages->SetLanguage(TEXT("en-US")));
   auto* BarClass=LoadObject<UDialogueBlueprintGeneratedClass>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Bar_NPC.L2_Bar_NPC_C"));check(BarClass);
   auto* Bar=NewObject<UDialogue>(GI,BarClass);BarClass->InitializeDialogue(Bar);
   TArray<UObject*> BarObjects;GetObjectsWithOuter(Bar,BarObjects,true);UDialogueNode* CheckNode=nullptr;
   for(auto* O:BarObjects)if(O->GetFName()==TEXT("DialogueNode_NPC_16"))CheckNode=Cast<UDialogueNode>(O);check(CheckNode);
   const FText BarText=CheckNode->Line.Text;const FString English=BarText.ToString();check(English!=*FTextInspector::GetSourceString(BarText));
   auto& Manager=FTextLocalizationManager::Get();Manager.RefreshResources();Manager.WaitForAsyncTasks();
   check(Languages->SetLanguage(TEXT("en-US")));check(BarText.ToString()==English);
   FString CompileError;checkf(VTGManualLocalization::Compile(CompileError),TEXT("%s"),*CompileError);
   check(Languages->GetLanguage()==TEXT("en-US") && BarText.ToString()==English);
   UE_LOG(LogTemp,Display,TEXT("BAR_DIALOGUE_LOCALIZATION_COMPILE_PASS: real NPC_16 runtime text remains English after reselect and live Compile"));
   check(Languages->SetLanguage(TEXT("zh-Hans")) && Label->GetText().ToString()==TEXT("Language"));
   Widget->ReleaseSlateResources(true);
   GI->RemoveLocalPlayer(Local);GI->Shutdown();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
   if(HadLanguage)GConfig->SetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),*SavedLanguage,GGameUserSettingsIni);else GConfig->RemoveKey(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
   UE_LOG(LogTemp,Display,TEXT("PAUSE_LANGUAGE_RUNTIME_PASS: actual widget button, matching style/font, order, en/zh clicks and external refresh without world ticks; user language restored"));
  }
  UE_LOG(LogTemp,Display,TEXT("PAUSE_LANGUAGE_ASSET_PASS verify=%d"),Verify);
  if(!Params.Contains(TEXT("Preview=")))return 0;
 }
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
