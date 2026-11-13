#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Font.h"
#include "Engine/DataTable.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/Overlay.h"
#include "Styling/SlateTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UI/VTGTerminalSaveLibrary.h"
#include "UI/VTGTerminalSavePage.h"
#include "VTGGameInstanceBase.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Border.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "UObject/StructOnScope.h"
#include "CommonTextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/LatentActionManager.h"
#include "TimerManager.h"

namespace TerminalStyle
{
const FString Folder=TEXT("Scripts/TerminalStyle/");
const FString UI=TEXT("/Game/Inventory/InventoryXBased/UMG/");
FString FontOwner;
void Save(UObject* O)
{
 const FString Package=O->GetOutermost()->GetName();check(Package.StartsWith(TEXT("/Game/")));
 const FString File=FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectDir()/Folder/TEXT("Backup")/(Package.Mid(6)+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 O->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(O->GetOutermost(),O,*File,A));
}
void Compile(UBlueprint* BP)
{
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog R;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&R);check(!R.NumErrors && BP->Status!=BS_Error);
}
UFont* FontFor(const FSlateFontInfo& Font)
{
 const FString Key=(Font.TypefaceFontName.ToString()+TEXT(" ")+GetNameSafe(Font.FontObject)).ToLower();FString Weight=TEXT("Regular");
 if(Key.Contains(TEXT("extralight")))Weight=TEXT("ExtraLight");else if(Key.Contains(TEXT("light")))Weight=TEXT("Light");else if(Key.Contains(TEXT("thin")))Weight=TEXT("Thin");
 else if(Key.Contains(TEXT("semibold")) || Key.Contains(TEXT("semi bold")))Weight=TEXT("SemiBold");else if(Key.Contains(TEXT("extrabold")))Weight=TEXT("ExtraBold");else if(Key.Contains(TEXT("black")))Weight=TEXT("Black");else if(Key.Contains(TEXT("bold")))Weight=TEXT("Bold");else if(Key.Contains(TEXT("medium")))Weight=TEXT("Medium");
 auto* Result=LoadObject<UFont>(nullptr,*(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-")+Weight+TEXT("_Font")));check(Result);return Result;
}
int32 Fonts(FProperty* P,void* Address,bool Verify)
{
 int32 Changed=0;
 if(auto* S=CastField<FStructProperty>(P))
 {
  if(S->Struct==FSlateFontInfo::StaticStruct())
  {
   auto* Font=static_cast<FSlateFontInfo*>(Address);if(Font->Size<=0)return 0;auto* Desired=FontFor(*Font);
   if(Font->FontObject!=Desired || !Font->TypefaceFontName.IsNone())
   {checkf(!Verify,TEXT("Terminal font is not migrated on %s: %s -> %s"),*FontOwner,*GetNameSafe(Font->FontObject),*Desired->GetName());Font->FontObject=Desired;Font->TypefaceFontName=NAME_None;return 1;}
  }
  else for(TFieldIterator<FProperty> F(S->Struct);F;++F)Changed+=Fonts(*F,F->ContainerPtrToValuePtr<void>(Address),Verify);
 }
 else if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,Address);for(int I=0;I<H.Num();++I)Changed+=Fonts(A->Inner,H.GetRawPtr(I),Verify);}
 return Changed;
}
int32 FontsOn(UObject* O,bool Verify)
{
 FontOwner=O->GetPathName();int32 Changed=0;for(TFieldIterator<FProperty> P(O->GetClass());P;++P)if(!P->HasAnyPropertyFlags(CPF_Transient))Changed+=Fonts(*P,P->ContainerPtrToValuePtr<void>(O),Verify);return Changed;
}
void Layout(bool Verify)
{
 auto* Mail=LoadObject<UWidgetBlueprint>(nullptr,*(UI+TEXT("TabSwitcher/InTabWidget/WB_TabWidget_Mail")));auto* Doc=LoadObject<UWidgetBlueprint>(nullptr,*(UI+TEXT("WB_NoteMainWidget")));check(Mail && Doc);
 auto* Reference=CastChecked<UCanvasPanelSlot>(Mail->WidgetTree->FindWidget(TEXT("Overlay_1"))->Slot);auto* Actual=CastChecked<UCanvasPanelSlot>(Doc->WidgetTree->FindWidget(TEXT("Overlay"))->Slot);
 if(!Verify){Actual->SetLayout(Reference->GetLayout());Compile(Doc);Save(Doc);}
 check(Actual->GetLayout().Offsets==Reference->GetLayout().Offsets && Actual->GetLayout().Anchors==Reference->GetLayout().Anchors);
 UE_LOG(LogTemp,Display,TEXT("TERMINAL_LAYOUT_PASS: Document uses the Mail/Quest/Map reference rectangle"));
}
void MigrateFonts(bool Verify)
{
 TArray<FAssetData> Assets;FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(*UI.LeftChop(1)),Assets,true);
 TArray<UWidgetBlueprint*> Queue;TSet<UWidgetBlueprint*> Seen;TSet<UDataTable*> Tables;TSet<UBlueprint*> TextStyles;
 auto Add=[&](UWidgetBlueprint* BP){if(BP && BP->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) && !Seen.Contains(BP)){Seen.Add(BP);Queue.Add(BP);}};
 for(const auto& A:Assets)if(A.AssetClassPath.GetAssetName()==TEXT("WidgetBlueprint"))Add(Cast<UWidgetBlueprint>(A.GetAsset()));
 int32 Total=0;
 for(int I=0;I<Queue.Num();++I)
 {
  auto* BP=Queue[I];int32 Changed=FontsOn(BP->GeneratedClass->GetDefaultObject(),Verify);
  BP->WidgetTree->ForEachWidget([&](UWidget* W)
  {
   // CommonTextBlock serializes its style reference instead of its resolved font.
   // Give external/native styles a terminal-owned child so reload keeps the font.
   for(TFieldIterator<FClassProperty> P(W->GetClass());P;++P)if(auto* C=Cast<UClass>(P->GetObjectPropertyValue_InContainer(W));C && C->IsChildOf(UCommonTextStyle::StaticClass()))
   {
    auto* Style=Cast<UBlueprint>(C->ClassGeneratedBy);
    if(!Style || !Style->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
    {
     checkf(!Verify,TEXT("Unmigrated CommonUI style %s"),*C->GetPathName());
     const FString Name=TEXT("ST_Terminal_")+C->GetName();const FString Path=UI+TEXT("Styles/")+Name;
     Style=LoadObject<UBlueprint>(nullptr,*Path);
     if(!Style)Style=FKismetEditorUtilities::CreateBlueprint(C,CreatePackage(*Path),*Name,BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
     check(Style);Compile(Style);P->SetObjectPropertyValue_InContainer(W,Style->GeneratedClass);++Changed;
    }
    TextStyles.Add(Style);
    if(!Verify){FontsOn(Style->GeneratedClass->GetDefaultObject(),false);Compile(Style);Save(Style);}
   }
   Changed+=FontsOn(W,Verify);if(W->Slot)Changed+=FontsOn(W->Slot,Verify);
   if(auto* Child=Cast<UUserWidget>(W))Add(Cast<UWidgetBlueprint>(Child->GetClass()->ClassGeneratedBy));
   for(TFieldIterator<FClassProperty> P(W->GetClass());P;++P)if(auto* C=Cast<UClass>(P->GetObjectPropertyValue_InContainer(W));C && C->IsChildOf(UCommonTextStyle::StaticClass()))
    if(auto* Style=Cast<UBlueprint>(C->ClassGeneratedBy);Style && Style->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))TextStyles.Add(Style);
   if(auto* Rich=Cast<URichTextBlock>(W))if(auto* Table=Rich->GetTextStyleSet())
   {
    if(!Table->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
    {
     check(!Verify);const FString Path=UI+TEXT("Styles/DT_Terminal_")+Table->GetName();auto* Package=CreatePackage(*Path);Table=DuplicateObject<UDataTable>(Table,Package,*FPaths::GetBaseFilename(Path));Table->SetFlags(RF_Public|RF_Standalone);Rich->SetTextStyleSet(Table);++Changed;
    }
    if(!Tables.Contains(Table))
    {
     Tables.Add(Table);int32 N=0;for(const auto& Row:Table->GetRowMap())for(TFieldIterator<FProperty> P(Table->GetRowStruct());P;++P)N+=Fonts(*P,P->ContainerPtrToValuePtr<void>(Row.Value),Verify);
     if(N){Total+=N;Save(Table);}
    }
   }
  });
  for(TFieldIterator<FClassProperty> P(BP->GeneratedClass);P;++P)if(auto* C=Cast<UClass>(P->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));C && C->IsChildOf(UUserWidget::StaticClass()))Add(Cast<UWidgetBlueprint>(C->ClassGeneratedBy));
  Total+=Changed;
 }
 // A nested user widget can expose its class template during traversal. Save
 // every reached owner after migration, including classes changed through a parent.
 if(!Verify)for(auto* BP:Queue){Compile(BP);Save(BP);}
 for(auto* Style:TextStyles){const int32 N=FontsOn(Style->GeneratedClass->GetDefaultObject(),Verify);if(N){Total+=N;Save(Style);}}
 UE_LOG(LogTemp,Display,TEXT("TERMINAL_FONTS_PASS: checked %d widgets, %d rich-text tables and %d CommonUI styles, changed %d font styles"),Queue.Num(),Tables.Num(),TextStyles.Num(),Total);
}
void Audit()
{
 FString Out;
 auto Properties=[&](UObject* O)
 {
  if(!O)return;Out+=TEXT("OBJECT ")+O->GetPathName()+TEXT(" CLASS ")+O->GetClass()->GetName()+TEXT("\n");
  for(TFieldIterator<FProperty> P(O->GetClass());P;++P)
  {
   const FString N=P->GetName();
   if(!(N.Contains(TEXT("Font")) || N==TEXT("Tabs") || N==TEXT("LayoutData") || N.Contains(TEXT("Padding")) || N.Contains(TEXT("Brush")) || N.Contains(TEXT("Style")) || N==TEXT("Text") || N==TEXT("Visibility") || N.Contains(TEXT("Alignment")) || N.Contains(TEXT("RenderTransform"))))continue;
   FString V;P->ExportText_InContainer(0,V,O,O,O,PPF_None);if(V.Len()<10000)Out+=N+TEXT("=")+V+TEXT("\n");
  }
 };
 for(const TCHAR* P:{TEXT("WB_Document"),TEXT("WB_NoteMainWidget"),TEXT("WB_Note"),TEXT("WB_TextContainer"),TEXT("TabSwitcher/WB_TabSwitcher"),TEXT("TabSwitcher/WB_TabSwitcherMenu"),TEXT("TabSwitcher/WB_TabButton"),TEXT("TabSwitcher/InTabWidget/WB_TabWidget_Quest"),TEXT("TabSwitcher/InTabWidget/WB_TabWidget_Map"),TEXT("TabSwitcher/InTabWidget/WB_TabWidget_Mail"),TEXT("Inventory/Background/WB_Background")})
 {
  auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*(UI+P));check(BP);Out+=TEXT("\nASSET ")+BP->GetPathName()+TEXT("\n");Properties(BP->GeneratedClass->GetDefaultObject());
  BP->WidgetTree->ForEachWidget([&](UWidget* W){Out+=TEXT("PARENT ")+GetNameSafe(W->GetParent())+TEXT("\n");Properties(W);Properties(W->Slot);});
 }
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir()/Folder),true);check(FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectDir()/Folder/TEXT("audit.txt"))));
 UE_LOG(LogTemp,Display,TEXT("TERMINAL_STYLE_AUDIT_SAVED"));
}
void Preview(bool Render)
{
 FEditorScriptExecutionGuard Guard;
 // Offscreen commandlets have no normal viewport streaming loop. Load the real
 // tab textures before their asynchronous UImage bindings are constructed.
 TArray<FAssetData> Textures;FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(*(UI+TEXT("Textures/Tabs"))),Textures,true);
 for(const auto& Texture:Textures)Texture.GetAsset();
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("TerminalStylePreview"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UVTGGameInstanceBase>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
 check(W->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute)));
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Pawn=W->SpawnActor<ACharacter>(LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C")),FTransform::Identity,Spawn);auto* PC=W->SpawnActor<APlayerController>();PC->Possess(Pawn);W->AddController(PC);auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);
 auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*(UI+TEXT("TabSwitcher/WB_TabSwitcher")));check(BP);auto* Terminal=CreateWidget<UUserWidget>(PC,BP->GeneratedClass.Get());check(Terminal);Terminal->SetVisibility(ESlateVisibility::SelfHitTestInvisible);auto Slate=Terminal->TakeWidget();
 auto* Menu=CastChecked<UUserWidget>(Terminal->GetWidgetFromName(TEXT("WB_TabSwitcher")));auto* Switcher=CastChecked<UWidgetSwitcher>(Terminal->GetWidgetFromName(TEXT("WidgetSwitcher_132")));
 auto* TabsProperty=FindFProperty<FArrayProperty>(Menu->GetClass(),TEXT("Tabs"));check(TabsProperty);FScriptArrayHelper Tabs(TabsProperty,TabsProperty->ContainerPtrToValuePtr<void>(Menu));
 UScriptStruct* TabStruct=CastFieldChecked<FStructProperty>(TabsProperty->Inner)->Struct;FByteProperty* TabIndex=nullptr;FSoftObjectProperty* TabIcon=nullptr;
 for(TFieldIterator<FProperty> P(TabStruct);P;++P){if(P->GetName().StartsWith(TEXT("WidgetIndex_")))TabIndex=CastField<FByteProperty>(*P);if(P->GetName().StartsWith(TEXT("Image_")))TabIcon=CastField<FSoftObjectProperty>(*P);}
 check(TabIndex && TabIcon && Tabs.Num()==4);FSoftObjectPtr DocumentIcon(FSoftObjectPath(TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Tabs/T_Note.T_Note"))),SaveIcon;
 for(int N=0;N<Tabs.Num();++N){const auto Index=TabIndex->GetPropertyValue_InContainer(Tabs.GetRawPtr(N));if(Index==0)DocumentIcon=TabIcon->GetPropertyValue_InContainer(Tabs.GetRawPtr(N));if(Index==254)SaveIcon=TabIcon->GetPropertyValue_InContainer(Tabs.GetRawPtr(N));}
 check(!DocumentIcon.IsNull() && DocumentIcon==SaveIcon && DocumentIcon.LoadSynchronous());
 // A legacy request for Document must select Quest without removing page zero.
 auto* SelectTab=Terminal->FindFunctionChecked(TEXT("SetActiveTab"));FStructOnScope SelectArgs(SelectTab);
 auto* SelectIndex=FindFProperty<FByteProperty>(SelectTab,TEXT("Index"));check(SelectIndex);SelectIndex->SetPropertyValue_InContainer(SelectArgs.GetStructMemory(),0);Terminal->ProcessEvent(SelectTab,SelectArgs.GetStructMemory());
 check(Switcher->GetChildAt(0) && Switcher->GetActiveWidgetIndex()==1);
 auto* Click=Menu->FindFunctionChecked(TEXT("OnClickButton_Event"));
 const int32 Indices[]={1,2,3,254};const TCHAR* Names[]={TEXT("quest"),TEXT("map"),TEXT("mail"),TEXT("save-load")};
 for(int I=0;I<4;++I)
 {
  FStructOnScope Args(Click);auto* Index=FindFProperty<FByteProperty>(Click,TEXT("Index"));check(Index);Index->SetPropertyValue_InContainer(Args.GetStructMemory(),Indices[I]);Menu->ProcessEvent(Click,Args.GetStructMemory());
  if(I<3)Switcher->SetActiveWidgetIndex(Indices[I]);
  if(I==3)
  {
   // Also exercise the native route directly: repeat selection must reuse one page.
   check(UVTGTerminalSaveLibrary::HandleTerminalSave(Terminal,254));auto* Page=CastChecked<UVTGTerminalSavePage>(Switcher->GetActiveWidget());Page->TakeWidget();
   auto* Artwork=CastChecked<UImage>(Page->GetWidgetFromName(TEXT("TerminalArtwork")));check(Artwork->GetBrush().GetResourceObject() && Artwork->GetBrush().GetResourceObject()->GetName()==TEXT("terminalBG_a"));
   auto* Screen=CastChecked<UBorder>(Page->GetWidgetFromName(TEXT("TerminalScreen")));check(Screen->GetBrushColor().A==0);
   auto* Text=CastChecked<UTextBlock>(Page->GetWidgetFromName(TEXT("SaveHeading")));check(GetNameSafe(Text->GetFont().FontObject)==TEXT("NotoSansSC-SemiBold_Font"));
  }
  if(Render)
  {
   FWidgetRenderer Renderer(true);auto* Target=FWidgetRenderer::CreateTargetFor(FVector2D(1920,1080),TF_Bilinear,true);check(Target);
   for(int Frame=0;Frame<45;++Frame){++GFrameCounter;FlushAsyncLoading();FTSTicker::GetCoreTicker().Tick(1.f/30);W->GetTimerManager().Tick(1.f/30);W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(nullptr,1.f/30);Renderer.DrawWidget(Target,Slate,FVector2D(1920,1080),1.f/30);FlushRenderingCommands();}
   FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectDir()/Folder/(FString(Names[I])+TEXT(".png")))));
  }
 }
 GI->RemoveLocalPlayer(Local);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
 UE_LOG(LogTemp,Display,TEXT("TERMINAL_STYLE_RUNTIME_PASS: real tabs, Document artwork and bounds, transparent save screen, weighted Noto fonts"));
}
}
int32 VTGTerminalStyleMaintenance(const FString& Params)
{
 if(FParse::Param(*Params,TEXT("Audit"))){TerminalStyle::Audit();return 0;}
 const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));TerminalStyle::Layout(Verify);TerminalStyle::MigrateFonts(Verify);TerminalStyle::Preview(FParse::Param(*Params,TEXT("Preview")));return 0;
}
