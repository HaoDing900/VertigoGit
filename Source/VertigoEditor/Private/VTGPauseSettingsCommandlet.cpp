#include "VTGPauseSettingsCommandlet.h"
#include "UI/VTGPauseMenu.h"
#include "UI/VTGLanguageSettingsPage.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "UI/VTGDialogueHistorySubsystem.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Factories/TextureFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Widgets/SWindow.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"

UVTGPauseSettingsCommandlet::UVTGPauseSettingsCommandlet()
{IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}

int32 UVTGPauseSettingsCommandlet::Main(const FString& Params)
{
 FEditorScriptExecutionGuard Guard;
 auto Justify=[](const UTextBlock* Text)
 {
  auto* Property=FindFProperty<FByteProperty>(Text->GetClass(),TEXT("Justification"));check(Property);
  return ETextJustify::Type(Property->GetPropertyValue_InContainer(Text));
 };
 const FString BPPath=TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu");
 const FString FramePath=TEXT("/Game/2DArt/UI/GameOption/ExtendedUIBox");
 auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*BPPath);check(BP);
 if(FParse::Param(*Params,TEXT("AuthorPauseLayout")))
 {
  auto* Continue=CastChecked<UButton>(BP->WidgetTree->FindWidget(TEXT("B_Continue")));
  auto* Exit=CastChecked<UButton>(BP->WidgetTree->FindWidget(TEXT("B_Exit")));
  auto* List=CastChecked<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("VerticalBox_137")));
  auto* ContinueLabel=CastChecked<UTextBlock>(Continue->GetContent());
  auto* Language=Cast<UButton>(BP->WidgetTree->FindWidget(TEXT("B_Language")));
  BP->Modify();
  if(!Language)
  {
   Language=BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("B_Language"));
   Language->SetStyle(Continue->GetStyle());Language->SetColorAndOpacity(Continue->GetColorAndOpacity());Language->SetBackgroundColor(Continue->GetBackgroundColor());
   auto* Text=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("LanguageEntryText"));
   Text->SetText(FText::AsCultureInvariant(TEXT("Language")));Text->SetFont(ContinueLabel->GetFont());Text->SetColorAndOpacity(ContinueLabel->GetColorAndOpacity());
   Text->SetShadowColorAndOpacity(ContinueLabel->GetShadowColorAndOpacity());Text->SetShadowOffset(ContinueLabel->GetShadowOffset());Text->SetJustification(Justify(ContinueLabel));
   Language->SetContent(Text);
   auto* ReferenceTextSlot=CastChecked<UButtonSlot>(ContinueLabel->Slot);auto* NewTextSlot=CastChecked<UButtonSlot>(Text->Slot);
   NewTextSlot->SetPadding(ReferenceTextSlot->GetPadding());NewTextSlot->SetHorizontalAlignment(ReferenceTextSlot->GetHorizontalAlignment());NewTextSlot->SetVerticalAlignment(ReferenceTextSlot->GetVerticalAlignment());
   auto* ExitSlot=CastChecked<UVerticalBoxSlot>(Exit->Slot);
   const FMargin RowPadding=ExitSlot->GetPadding();const auto RowSize=ExitSlot->GetSize();const auto Horizontal=ExitSlot->GetHorizontalAlignment();const auto Vertical=ExitSlot->GetVerticalAlignment();
   List->RemoveChild(Exit);
   for(auto* Button:{Language,Exit})
   {
    auto* Row=List->AddChildToVerticalBox(Button);Row->SetPadding(RowPadding);Row->SetSize(RowSize);Row->SetHorizontalAlignment(Horizontal);Row->SetVerticalAlignment(Vertical);
   }
  }
  // Persist only the requested font family. Authored font sizes, alignment,
  // title position, anchors, padding and button geometry remain the source of truth.
  for(auto* Button:{Continue,Language,Exit})
  {
   auto* Text=CastChecked<UTextBlock>(Button->GetContent());auto Info=Text->GetFont();
   Info.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));Info.TypefaceFontName=NAME_None;Text->SetFont(Info);
  }
  auto* Heading=CastChecked<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("TextBlock_81")));
  auto Info=Heading->GetFont();Info.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-SemiBold_Font"));Info.TypefaceFontName=NAME_None;Heading->SetFont(Info);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BPPath,FPackageName::GetAssetPackageExtension()),Args));
  UE_LOG(LogTemp,Display,TEXT("PAUSE_AUTHORED_LAYOUT: heading font=%.0f button font=%.0f; Language saved in WBP; original geometry retained"),double(Heading->GetFont().Size),double(ContinueLabel->GetFont().Size));
 }
 const FString MainFile=FPaths::ProjectContentDir()/TEXT("ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu.uasset");
 const auto MainHash=FMD5Hash::HashFile(*MainFile);
 FString Source;
 if(FParse::Value(*Params,TEXT("ImportFrame="),Source))
 {
  check(BP->GeneratedClass->IsChildOf(UVTGPauseMenu::StaticClass()));
  auto* Texture=LoadObject<UTexture2D>(nullptr,*FramePath,nullptr,LOAD_NoWarn);
  if(!Texture)
  {
   auto* Package=CreatePackage(*FramePath);
   auto* Factory=NewObject<UTextureFactory>();Factory->SuppressImportOverwriteDialog();
   bool Canceled=false;
   Texture=Cast<UTexture2D>(Factory->FactoryCreateFile(UTexture2D::StaticClass(),Package,TEXT("ExtendedUIBox"),RF_Public|RF_Standalone,Source,nullptr,GWarn,Canceled));
   check(Texture && !Canceled);
   Texture->LODGroup=TEXTUREGROUP_UI;Texture->CompressionSettings=TC_EditorIcon;Texture->MipGenSettings=TMGS_NoMipmaps;Texture->SRGB=true;Texture->UpdateResource();
   FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
   check(UPackage::SavePackage(Package,Texture,*FPackageName::LongPackageNameToFilename(FramePath,FPackageName::GetAssetPackageExtension()),Args));
  }
  // A serialized hard reference on the BP default guarantees the frame is cooked.
  CastChecked<UVTGPauseMenu>(BP->GeneratedClass->GetDefaultObject())->SettingsFrame=Texture;
  BP->Modify();FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
  FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);
  check(CastChecked<UVTGPauseMenu>(BP->GeneratedClass->GetDefaultObject())->SettingsFrame==Texture);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BPPath,FPackageName::GetAssetPackageExtension()),Args));
 }
 check(CastChecked<UVTGPauseMenu>(BP->GeneratedClass->GetDefaultObject())->SettingsFrame);
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 FString SavedLanguage;const bool HadLanguage=GConfig->GetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),SavedLanguage,GGameUserSettingsIni);
 FParse::Value(*Params,TEXT("RestoreLanguage="),SavedLanguage);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("PauseSettingsTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* Dummy=GI->GetWorld();GI->GetWorldContext()->SetCurrentWorld(World);World->SetGameInstance(GI);Dummy->DestroyWorld(false);
 auto* Viewport=NewObject<UGameViewportClient>(GEngine);
 GI->GetWorldContext()->GameViewport=Viewport;Viewport->Init(*GI->GetWorldContext(),GI,false);
 auto* PC=World->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));PC->SetPlayer(Local);Local->PlayerController=PC;
 auto* Menu=CreateWidget<UVTGPauseMenu>(PC,BP->GeneratedClass.Get());check(Menu);
 auto Slate=Menu->TakeWidget();
 auto* LanguageButton=CastChecked<UButton>(Menu->GetWidgetFromName(TEXT("B_Language")));
 auto* Label=CastChecked<UTextBlock>(LanguageButton->GetContent());
 auto* List=CastChecked<UVerticalBox>(Menu->GetWidgetFromName(TEXT("VerticalBox_137")));
 auto* Languages=GI->GetSubsystem<UVTGLocalizationSubsystem>();check(Languages);
 // Check the actual WBP template against its constructed runtime instance.
 check(BP->WidgetTree->FindWidget(TEXT("B_Language")));
 for(const TCHAR* Name:{TEXT("TextBlock_81"),TEXT("TextBlock_206"),TEXT("TextBlock"),TEXT("LanguageEntryText")})
 {
  auto* Authored=CastChecked<UTextBlock>(BP->WidgetTree->FindWidget(Name));
  auto* Runtime=CastChecked<UTextBlock>(Menu->GetWidgetFromName(Name));
  check(Authored->GetFont()==Runtime->GetFont());check(Justify(Authored)==Justify(Runtime));
 }
 auto* AuthoredTitle=CastChecked<UCanvasPanelSlot>(BP->WidgetTree->FindWidget(TEXT("TextBlock_81"))->Slot);
 auto* RuntimeTitle=CastChecked<UCanvasPanelSlot>(Menu->GetWidgetFromName(TEXT("TextBlock_81"))->Slot);
 check(AuthoredTitle->GetPosition()==RuntimeTitle->GetPosition() && AuthoredTitle->GetSize()==RuntimeTitle->GetSize());
 check(AuthoredTitle->GetAlignment()==RuntimeTitle->GetAlignment() && AuthoredTitle->GetAnchors()==RuntimeTitle->GetAnchors());
 check(List->GetChildrenCount()==CastChecked<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("VerticalBox_137")))->GetChildrenCount());
 check(Label->GetText().ToString()==TEXT("Language"));
 const FString InitialLanguage=Languages->GetLanguage();
 LanguageButton->OnClicked.Broadcast();
 UE_LOG(LogTemp,Display,TEXT("PAUSE_PAGE_OPEN initial=%s actual=%s list=%d visibility=%d root=%s owner=%s gameinstance=%s enabled=%d"),*InitialLanguage,*Languages->GetLanguage(),List->IsVisible(),int32(List->GetVisibility()),*GetNameSafe(Menu->WidgetTree->RootWidget),*GetNameSafe(Menu->GetOwningPlayer()),*GetNameSafe(Menu->GetGameInstance()),LanguageButton->GetIsEnabled());
 check(Languages->GetLanguage()==InitialLanguage && List->IsVisible() && !List->GetIsEnabled());
 auto* PauseArt=Menu->GetWidgetFromName(TEXT("PauseArtwork"));check(PauseArt && PauseArt->IsVisible());
 check(Menu->GetWidgetFromName(TEXT("TextBlock_81"))->IsVisible());
 UVTGLanguageSettingsPage* Page=nullptr;
 Menu->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* P=Cast<UVTGLanguageSettingsPage>(Widget))Page=P;});check(Page);
 auto* Combo=CastChecked<UVTGLanguageComboBox>(Page->GetWidgetFromName(TEXT("LanguageSelection")));
 auto* Back=CastChecked<UButton>(Page->GetWidgetFromName(TEXT("LanguageBack")));
 check(Combo->GetOptionCount()==UVTGLocalizationSubsystem::GetLanguages().Num());
 check(Combo->FindOptionIndex(TEXT("English"))!=INDEX_NONE && Combo->FindOptionIndex(TEXT("简体中文"))!=INDEX_NONE);
 auto Root=SNew(SOverlay)
  +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.06,.10,.12,1))]
  +SOverlay::Slot()[Slate];
 auto Window=SNew(SWindow).ClientSize(FVector2D(1920,1080)).SizingRule(ESizingRule::FixedSize)[Root];
 FSlateApplication::Get().AddWindow(Window,false);
 FWidgetRenderer Renderer(false);
 FString Output;FParse::Value(*Params,TEXT("Output="),Output);
 if(!Output.IsEmpty())IFileManager::Get().MakeDirectory(*Output,true);
 auto Render=[&](const TCHAR* Name,FVector2D Size)
 {
  if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
  auto* Target=Renderer.DrawWidget(Root,Size);FlushRenderingCommands();
  for(int I=0;I<3;++I){Renderer.DrawWidget(Target,Root,Size,.016f);FlushRenderingCommands();}
  if(!Output.IsEmpty())
  {
   FReadSurfaceDataFlags ReadFlags(RCM_UNorm);ReadFlags.SetLinearToGamma(false);
   TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags));
   TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*(Output/Name)));
  }
 };
 Combo->SetSelectedOption(TEXT("English"));check(Languages->GetLanguage()==TEXT("en-US"));
 Render(TEXT("language-en.png"),FVector2D(1920,1080));
 // Highlighting an option is not a commit.
 Combo->OnSelectionChanged.Broadcast(TEXT("简体中文"),ESelectInfo::OnNavigation);check(Languages->GetLanguage()==TEXT("en-US"));
 Combo->SetSelectedOption(TEXT("简体中文"));check(Languages->GetLanguage()==TEXT("zh-Hans"));
 check(CastChecked<UTextBlock>(Page->GetWidgetFromName(TEXT("DisplayLanguageLabel")))->GetText().ToString()==TEXT("显示语言"));
 FString Persisted;check(GConfig->GetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),Persisted,GGameUserSettingsIni) && Persisted==TEXT("zh-Hans"));
 Render(TEXT("language-zh.png"),FVector2D(1920,1080));
 Render(TEXT("language-small.png"),FVector2D(1280,720));
 Combo->SetMenuOpen(true);check(Combo->IsOpen());
 check(Menu->HandleSettingsBack() && !Combo->IsOpen() && Page->IsVisible() && List->IsVisible() && !List->GetIsEnabled());
 check(Menu->HandleSettingsBack() && !Page->IsVisible() && List->IsVisible() && List->GetIsEnabled());
 check(LanguageButton->HasKeyboardFocus());check(!Menu->HandleSettingsBack());
 LanguageButton->OnClicked.Broadcast();check(Page->IsVisible() && Combo->GetSelectedOption()==TEXT("简体中文"));
 Back->OnClicked.Broadcast();check(List->IsVisible() && List->GetIsEnabled() && !Page->IsVisible() && Languages->GetLanguage()==TEXT("zh-Hans"));
 Render(TEXT("pause-main.png"),FVector2D(1920,1080));
 LanguageButton->OnClicked.Broadcast();
 for(int32 Cycle=0;Cycle<3;++Cycle)
 {
  check(Languages->SetLanguage(TEXT("en-US")));check(Combo->GetSelectedOption()==TEXT("English"));
  check(Languages->SetLanguage(TEXT("zh-Hans")));check(Combo->GetSelectedOption()==TEXT("简体中文"));
 }
 // Reconstructing the widget reads the saved language and preserves the fixed entry text.
 auto* Reopened=CreateWidget<UVTGPauseMenu>(PC,BP->GeneratedClass.Get());auto ReopenSlate=Reopened->TakeWidget();
 auto* ReopenButton=CastChecked<UButton>(Reopened->GetWidgetFromName(TEXT("B_Language")));
 check(CastChecked<UTextBlock>(ReopenButton->GetContent())->GetText().ToString()==TEXT("Language"));
 ReopenButton->OnClicked.Broadcast();
 UVTGLanguageSettingsPage* ReopenPage=nullptr;
 Reopened->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* P=Cast<UVTGLanguageSettingsPage>(Widget))ReopenPage=P;});check(ReopenPage);
 check(CastChecked<UVTGLanguageComboBox>(ReopenPage->GetWidgetFromName(TEXT("LanguageSelection")))->GetSelectedOption()==TEXT("简体中文"));
 check(MainHash==FMD5Hash::HashFile(*MainFile));
 FSlateApplication::Get().RequestDestroyWindow(Window);
 Menu->ReleaseSlateResources(true);Reopened->ReleaseSlateResources(true);
 GI->RemoveLocalPlayer(Local);
 Viewport->DetachViewportClient();GI->GetWorldContext()->GameViewport=nullptr;
 GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 if(HadLanguage)GConfig->SetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),*SavedLanguage,GGameUserSettingsIni);else GConfig->RemoveKey(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),GGameUserSettingsIni);
 GConfig->Flush(false,GGameUserSettingsIni);
 UE_LOG(LogTemp,Display,TEXT("PAUSE_SETTINGS_PASS: WBP/runtime fonts and title geometry identical; Language authored in WBP; Pause artwork/title retained under overlay; underlying controls disabled/restored; dropdown en/zh, save, back, focus, reopen verified; original preference restored"));
 return 0;
}
