#include "UI/VTGTerminalSavePage.h"
#include "Save/VTGSaveDisplayNames.h"
#include "Save/VTGSaveCoordinator.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "Engine/Font.h"
#include "Components/BackgroundBlur.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

void UVTGTerminalSlotButton::Setup(UVTGTerminalSavePage* Page,int32 Index)
{ PageOwner=Page;SlotIndex=Index;IsFocusable=false;OnClicked.AddUniqueDynamic(this,&UVTGTerminalSlotButton::Select); }
void UVTGTerminalSlotButton::Select(){if(PageOwner)PageOwner->SelectSlot(SlotIndex);}

UTextBlock* UVTGTerminalSavePage::Text(FName Name,const FText& Value,int32 Size)
{
 auto* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
 Label->SetText(Value);auto Font=Label->GetFont();Font.Size=Size;Font.TypefaceFontName=NAME_None;
 Font.FontObject=LoadObject<UFont>(nullptr,Name==TEXT("SaveHeading")?TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-SemiBold_Font"):TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));Label->SetFont(Font);
 Label->SetColorAndOpacity(FSlateColor(FLinearColor(.95f,.94f,.90f)));Label->SetAutoWrapText(true);return Label;
}
namespace
{
 /** Slot 0 is the combat checkpoint, the story autosave slot is the game's; both are read-only here. */
 bool VTGSavePageIsManualSlot(const UObject* Context,int32 Slot)
 {
  auto* GI=Context?Context->GetWorld()?Context->GetWorld()->GetGameInstance():nullptr:nullptr;
  auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;
  return Save?Save->IsManualSlot(Slot):Slot>0;
 }
 bool VTGSavePageIsStorySlot(const UObject* Context,int32 Slot)
 {
  auto* GI=Context?Context->GetWorld()?Context->GetWorld()->GetGameInstance():nullptr:nullptr;
  auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;
  return Save && Slot==Save->GetStoryAutoSaveSlot();
 }
}

UButton* UVTGTerminalSavePage::Action(FName Name,const FText& Label)
{
 auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);Button->IsFocusable=false;
 FButtonStyle Style=Button->GetStyle();Style.Normal.TintColor=FSlateColor(FLinearColor(.09f,.08f,.07f,.82f));
 Style.Hovered.TintColor=FSlateColor(FLinearColor(.20f,.18f,.16f,.9f));Style.Pressed.TintColor=FSlateColor(FLinearColor(.06f,.05f,.04f,.9f));
 Style.NormalPadding=FMargin(20,12);Style.PressedPadding=FMargin(20,14,20,10);Button->SetStyle(Style);
 auto* Caption=Text(NAME_None,Label,20);auto Font=Caption->GetFont();Font.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));Caption->SetFont(Font);Caption->SetJustification(ETextJustify::Center);Button->SetContent(Caption);return Button;
}
TSharedRef<SWidget> UVTGTerminalSavePage::RebuildWidget()
{
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
 if(!WidgetTree->RootWidget)
 {
  auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
  Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  auto* Screen=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("TerminalScreen"));
  Screen->SetBrushColor(FLinearColor::Transparent);Screen->SetPadding(FMargin(28));Screen->SetClipping(EWidgetClipping::ClipToBoundsAlways);
  auto* ScreenSlot=Canvas->AddChildToCanvas(Screen);
  if(!HasScreenLayout){ScreenLayout.Anchors=FAnchors(.5f,.5f);ScreenLayout.Offsets=FMargin(-580,-340,1152,681.5f);}
  ScreenSlot->SetLayout(ScreenLayout);
  // The page is an Overlay, not the body directly, so an unavailable message can take the screen the
  // same way the Mail tab's "No Message." does: centred, 20px down from the top of the content.
  auto* Stack=WidgetTree->ConstructWidget<UOverlay>();Screen->SetContent(Stack);
  auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();
  if(auto* BodySlot=Stack->AddChildToOverlay(Body))
  {BodySlot->SetHorizontalAlignment(HAlign_Fill);BodySlot->SetVerticalAlignment(VAlign_Fill);}
  // Matched to WB_TabWidget_Mail's "NoMessage": NotoSansSC-Medium at 25, centre/centre, top pad 20.
  Unavailable=Text(TEXT("SaveUnavailable"),NSLOCTEXT("VTGTerminal","InConflict","Not available within conflict."),25);
  {
   auto Font=Unavailable->GetFont();
   Font.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));
   Unavailable->SetFont(Font);Unavailable->SetAutoWrapText(false);
  }
  Unavailable->SetVisibility(ESlateVisibility::Collapsed);
  if(auto* MessageSlot=Stack->AddChildToOverlay(Unavailable))
  {MessageSlot->SetHorizontalAlignment(HAlign_Center);MessageSlot->SetVerticalAlignment(VAlign_Center);MessageSlot->SetPadding(FMargin(0,20,0,0));}
  Body->AddChildToVerticalBox(Text(TEXT("SaveHeading"),NSLOCTEXT("VTGTerminal","SaveLoad","SAVE / LOAD"),28))->SetPadding(FMargin(0,0,0,8));
  Body->AddChildToVerticalBox(Text(NAME_None,NSLOCTEXT("VTGTerminal","SlotHelp","9 manual slots. Automatic checkpoints are protected."),17))->SetPadding(FMargin(0,0,0,18));
  PageBody=Body;
  auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();auto* MainSlot=Body->AddChildToVerticalBox(Columns);MainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  SlotList=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("SaveSlots"));SlotList->SetClipping(EWidgetClipping::ClipToBoundsAlways);SlotList->SetAlwaysShowScrollbar(true);
  auto* ListSlot=Columns->AddChildToHorizontalBox(SlotList);ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));ListSlot->SetPadding(FMargin(0,0,24,0));
  auto* Details=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SaveDetailPanel"));Details->SetBrushColor(FLinearColor(.018f,.014f,.010f,.55f));Details->SetPadding(FMargin(24));
  Columns->AddChildToHorizontalBox(Details)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  DetailText=Text(TEXT("SaveDetails"),FText::GetEmpty(),21);Details->SetContent(DetailText);
  StatusText=Text(TEXT("SaveStatus"),NSLOCTEXT("VTGTerminal","ChooseSlot","Select a slot to save or load."),17);
  Body->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0,16,0,12));
  auto* Actions=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Actions);
  SaveButton=Action(TEXT("SaveGameButton"),NSLOCTEXT("VTGTerminal","SaveButton","SAVE GAME"));SaveButton->OnClicked.AddUniqueDynamic(this,&UVTGTerminalSavePage::SaveClicked);
  LoadButton=Action(TEXT("LoadGameButton"),NSLOCTEXT("VTGTerminal","LoadButton","LOAD GAME"));LoadButton->OnClicked.AddUniqueDynamic(this,&UVTGTerminalSavePage::LoadClicked);
  CancelButton=Action(TEXT("CancelSaveAction"),NSLOCTEXT("VTGTerminal","Cancel","CANCEL"));CancelButton->OnClicked.AddUniqueDynamic(this,&UVTGTerminalSavePage::CancelClicked);
  auto* Close=Action(TEXT("CloseSavePage"),NSLOCTEXT("VTGTerminal","Close","CLOSE"));Close->OnClicked.AddUniqueDynamic(this,&UVTGTerminalSavePage::CloseClicked);
  for(UButton* Button:{SaveButton.Get(),LoadButton.Get(),CancelButton.Get(),Close})Actions->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0,0,12,0));
 }
 RefreshTerminalFrame();RefreshSlots();return Super::RebuildWidget();
}
void UVTGTerminalSavePage::OpenForTerminal(UUserWidget* Terminal,UUserWidget* Source,UUserWidget* DocumentSource)
{
 if(auto* Previous=TerminalOwner.Get())Previous->OnVisibilityChanged.RemoveDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);
 TerminalOwner=Terminal;
 if(DocumentSource)
  if(auto* Image=Cast<UImage>(DocumentSource->GetWidgetFromName(TEXT("Image_AddTerminal"))))
   if(auto* Overlay=DocumentSource->GetWidgetFromName(TEXT("Overlay")))
    if(auto* Layout=Cast<UCanvasPanelSlot>(Overlay->Slot))
    {TerminalArtworkBrush=Image->GetBrush();TerminalArtworkLayout=Layout->GetLayout();HasTerminalArtwork=true;}
 if(Terminal)Terminal->OnVisibilityChanged.AddUniqueDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);
 // Use the actual mail content rectangle, not the full-screen widget switcher bounds.
  if(Source){
  if(auto* Background=Cast<UUserWidget>(Source->GetWidgetFromName(TEXT("WB_Background"))))
  {
   SharedBackgroundClass=Background->GetClass();
   Background->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* Blur=Cast<UBackgroundBlur>(Widget))if(auto* Slot=Cast<UCanvasPanelSlot>(Blur->Slot)){BlurSource=Blur;BlurLayout=Slot->GetLayout();}});
   if(auto* FrameImage=Cast<UImage>(Background->GetWidgetFromName(TEXT("Terminal"))))
    if(auto* FrameImageSlot=Cast<UCanvasPanelSlot>(FrameImage->Slot)){
     TerminalFrameBrush=FrameImage->GetBrush();TerminalFrameLayout=FrameImageSlot->GetLayout();HasTerminalFrame=true;
    }
  }
  RefreshTerminalFrame();
  auto* Frame=Source->GetWidgetFromName(TEXT("Overlay_1"));auto* Content=Source->GetWidgetFromName(TEXT("OverlayForMail"));
  auto* FrameSlot=Frame?Cast<UCanvasPanelSlot>(Frame->Slot):nullptr;auto* ContentSlot=Content?Cast<UOverlaySlot>(Content->Slot):nullptr;
  if(FrameSlot&&ContentSlot){
   ScreenLayout=FrameSlot->GetLayout();const auto P=ContentSlot->GetPadding();
   ScreenLayout.Offsets.Left+=P.Left;ScreenLayout.Offsets.Top+=P.Top;
   ScreenLayout.Offsets.Right-=P.Left+P.Right;ScreenLayout.Offsets.Bottom-=P.Top+P.Bottom;HasScreenLayout=true;
   if(auto* Screen=GetWidgetFromName(TEXT("TerminalScreen")))if(auto* S=Cast<UCanvasPanelSlot>(Screen->Slot))S->SetLayout(ScreenLayout);
  }
 }
 PendingAction=0;Busy=false;SetVisibility(ESlateVisibility::SelfHitTestInvisible);if(SlotList)RefreshSlots();
}
void UVTGTerminalSavePage::OpenForMainMenu(UUserWidget* Menu)
{
 if(auto* Previous=TerminalOwner.Get())Previous->OnVisibilityChanged.RemoveDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);
 TerminalOwner=nullptr;
 MenuOwner=Menu;
 LoadOnly=true;
 // There is no world and no pawn yet, so saving is meaningless here; SaveTerminalSlot would refuse
 // anyway. Hide the button instead of letting the player click into a guaranteed error.
 if(SaveButton)SaveButton->SetVisibility(ESlateVisibility::Collapsed);
 PendingAction=0;Busy=false;SelectedSlot=INDEX_NONE;
 SetVisibility(ESlateVisibility::SelfHitTestInvisible);
 if(SlotList)RefreshSlots();
}


void UVTGTerminalSavePage::RefreshAvailability()
{
 if(!Unavailable||!PageBody)return;
 auto* GI=GetGameInstance();auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 // Only combat blanks the page. Dialogue and cutscenes already keep the terminal from opening at all
 // (they own the UI input), and in main-menu mode there is no world to be in a fight in.
 const bool Blocked=!LoadOnly&&Save&&Save->IsPlayerInCombat(GetWorld());
 Unavailable->SetVisibility(Blocked?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 PageBody->SetVisibility(Blocked?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
}

void UVTGTerminalSavePage::RefreshSlots()
{
 if(!SlotList)return;
 auto* GI=GetGameInstance();auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 Metas.Reset();if(Save)Save->GetAllSlotMetas(Metas);
 if(Metas.IsEmpty())for(int32 I=0;I<10;++I){FVTGSlotMeta M;M.SlotIndex=I;Metas.Add(M);}
 if(SelectedSlot==INDEX_NONE){SelectedSlot=1;FDateTime Latest;for(const auto& M:Metas)if(M.bIsValid&&M.SaveTimeUtc>Latest){Latest=M.SaveTimeUtc;SelectedSlot=M.SlotIndex;}}
 SlotList->ClearChildren();SlotButtons.Reset();
 for(const auto& M:Metas){
  auto* Row=WidgetTree->ConstructWidget<UVTGTerminalSlotButton>();Row->Setup(this,M.SlotIndex);
  const FText Number=FText::AsCultureInvariant(FString::Printf(TEXT("%02d"),M.SlotIndex));
  const FText Title=M.SlotIndex==0?NSLOCTEXT("VTGTerminal","AutoSave","AUTO SAVE"):VTGSavePageIsStorySlot(this,M.SlotIndex)?NSLOCTEXT("VTGTerminal","StoryAutoSave","STORY AUTOSAVE"):FText::Format(NSLOCTEXT("VTGTerminal","ManualSlot","MANUAL {Slot}"),FFormatNamedArguments{{TEXT("Slot"),Number}});
  FText Summary=NSLOCTEXT("VTGTerminal","EmptySlot","Empty slot");
  // Place and phase come from the authored tables, never from the level or stage asset names.
  if(M.bIsValid)Summary=FText::Format(NSLOCTEXT("VTGTerminal","SlotSummary","{Level}  |  {Date}"),FFormatNamedArguments{{TEXT("Level"),UVTGSaveDisplayNames::ResolveLocation(M.LevelName)},{TEXT("Date"),FText::AsDateTime(M.SaveTimeUtc,EDateTimeStyle::Short,EDateTimeStyle::Short)}});
  Row->SetContent(Text(NAME_None,FText::Format(NSLOCTEXT("VTGTerminal","SlotRow","{Title}\n{Summary}"),FFormatNamedArguments{{TEXT("Title"),Title},{TEXT("Summary"),Summary}}),17));CastChecked<UButtonSlot>(Row->GetContent()->Slot)->SetHorizontalAlignment(HAlign_Fill);
  FButtonStyle Style=Row->GetStyle();Style.NormalPadding=FMargin(16,12);Style.PressedPadding=FMargin(16,14,16,10);Row->SetStyle(Style);
  SlotList->AddChild(Row);SlotButtons.Add(Row);
 }
 UpdateSelection();RefreshAvailability();
}
void UVTGTerminalSavePage::SelectSlot(int32 Index){if(Busy)return;SelectedSlot=Index;PendingAction=0;UpdateSelection();}
void UVTGTerminalSavePage::UpdateSelection()
{
 if(!SaveButton)return;
 const auto* M=Metas.FindByPredicate([&](const FVTGSlotMeta& Value){return Value.SlotIndex==SelectedSlot;});
 for(int32 I=0;I<SlotButtons.Num();++I){auto* B=SlotButtons[I].Get();auto Style=B->GetStyle();Style.Normal.TintColor=FSlateColor(Metas[I].SlotIndex==SelectedSlot?FLinearColor(.22f,.20f,.17f,.85f):FLinearColor(.025f,.024f,.022f,.60f));Style.Hovered.TintColor=FSlateColor(FLinearColor(.30f,.27f,.24f,.9f));B->SetStyle(Style);}
 const FText Heading=SelectedSlot==0?NSLOCTEXT("VTGTerminal","AutoCheckpoint","AUTO SAVE\n\nRead-only checkpoint")
  :VTGSavePageIsStorySlot(this,SelectedSlot)?NSLOCTEXT("VTGTerminal","StoryCheckpoint","STORY AUTOSAVE\n\nRead-only, written on entering a new stage")
  :FText::Format(NSLOCTEXT("VTGTerminal","ManualSaveHeading","MANUAL SAVE {Slot}"),FFormatNamedArguments{{TEXT("Slot"),FText::AsCultureInvariant(FString::Printf(TEXT("%02d"),SelectedSlot))}});
 FText Info;
 if(M&&M->bIsValid)
 {
  // The phase line is dropped when the stage has no row: an unmapped stage name is exactly what the
  // player must not see. DisplayLabel is gone from here - it only ever repeated the heading above it
  // in English, baked into the save file and therefore never translated.
  const FText Phase=UVTGSaveDisplayNames::ResolvePhase(M->Stage);
  Info=FText::Format(Phase.IsEmpty()
   ?NSLOCTEXT("VTGTerminal","ValidSlotDetail","{Heading}\n\n{Level}\n\n{Date}")
   :NSLOCTEXT("VTGTerminal","ValidSlotDetailPhase","{Heading}\n\n{Level}\n{Phase}\n\n{Date}"),
   FFormatNamedArguments{{TEXT("Heading"),Heading},{TEXT("Level"),UVTGSaveDisplayNames::ResolveLocation(M->LevelName)},{TEXT("Phase"),Phase},{TEXT("Date"),FText::AsDateTime(M->SaveTimeUtc,EDateTimeStyle::Medium,EDateTimeStyle::Short)}});
 }
 else Info=FText::Format(NSLOCTEXT("VTGTerminal","EmptySlotDetail","{Heading}\n\nEmpty slot\n\nSave your current position and progress here."),FFormatNamedArguments{{TEXT("Heading"),Heading}});
 DetailText->SetText(Info);
 SaveButton->SetIsEnabled(M&&VTGSavePageIsManualSlot(this,SelectedSlot)&&!Busy&&PendingAction!=2);LoadButton->SetIsEnabled(M&&M->bIsValid&&!Busy&&PendingAction!=1);
 CastChecked<UTextBlock>(SaveButton->GetContent())->SetText(PendingAction==1?NSLOCTEXT("VTGTerminal","Overwrite","CONFIRM SAVE"):NSLOCTEXT("VTGTerminal","SaveButton","SAVE GAME"));
 CastChecked<UTextBlock>(LoadButton->GetContent())->SetText(PendingAction==2?NSLOCTEXT("VTGTerminal","ConfirmLoad","CONFIRM LOAD"):NSLOCTEXT("VTGTerminal","LoadButton","LOAD GAME"));
 CancelButton->SetVisibility(PendingAction?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(!PendingAction){StatusText->SetText(LoadOnly?NSLOCTEXT("VTGTerminal","ChooseSlotLoadOnly","Select a save to continue."):NSLOCTEXT("VTGTerminal","ChooseSlot","Select a slot to save or load."));StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(.95f,.94f,.90f)));}
}
void UVTGTerminalSavePage::ShowResult(const FText& Message,bool Success)
{StatusText->SetText(Message);StatusText->SetColorAndOpacity(FSlateColor(Success?FLinearColor(.45f,1.f,.65f):FLinearColor(1.f,.65f,.4f)));}
void UVTGTerminalSavePage::SaveClicked()
{
 if(Busy||!VTGSavePageIsManualSlot(this,SelectedSlot))return;
 const auto* M=Metas.FindByPredicate([&](const FVTGSlotMeta& Value){return Value.SlotIndex==SelectedSlot;});if(!M)return;
 if(M->bIsValid&&PendingAction!=1){PendingAction=1;UpdateSelection();ShowResult(NSLOCTEXT("VTGTerminal","OverwriteWarning","Replace this save? Click CONFIRM SAVE, or Cancel."),false);return;}
 auto* GI=GetGameInstance();auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 FText Result=NSLOCTEXT("VTGTerminal","Unavailable","Saving is unavailable.");Busy=true;
 const bool Success=Save&&Save->SaveTerminalSlot(SelectedSlot,Result);
 Busy=false;PendingAction=0;RefreshSlots();ShowResult(Result,Success);
}
void UVTGTerminalSavePage::LoadClicked()
{
 if(Busy)return;
 const auto* M=Metas.FindByPredicate([&](const FVTGSlotMeta& Value){return Value.SlotIndex==SelectedSlot;});if(!M||!M->bIsValid)return;
 if(PendingAction!=2){PendingAction=2;UpdateSelection();ShowResult(NSLOCTEXT("VTGTerminal","LoadWarning","Load this save? Unsaved progress will be lost. Confirm or Cancel."),false);return;}
 auto* GI=GetGameInstance();auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;Busy=true;
 if(Save&&Save->LoadFromSlot(SelectedSlot)){
  if(auto* PC=GetOwningPlayer()){PC->SetPause(false);PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
  SetVisibility(ESlateVisibility::Collapsed);if(auto* Owner=TerminalOwner.Get())Owner->SetVisibility(ESlateVisibility::Collapsed);
  // Main-menu mode: the menu must not come back while the saved map is loading.
  if(LoadOnly){if(auto* Menu=MenuOwner.Get()){Menu->SetIsEnabled(false);Menu->SetVisibility(ESlateVisibility::Collapsed);}RemoveFromParent();}
 }else{Busy=false;PendingAction=0;UpdateSelection();ShowResult(NSLOCTEXT("VTGTerminal","LoadFailed","Could not load this save."),false);}
}
void UVTGTerminalSavePage::CancelClicked(){PendingAction=0;UpdateSelection();}
void UVTGTerminalSavePage::CloseClicked()
{
 PendingAction=0;SetVisibility(ESlateVisibility::Collapsed);
 if(LoadOnly)
 {
  // Opened over the main menu as its own viewport widget: give the menu the screen back.
  if(auto* Menu=MenuOwner.Get())Menu->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  RemoveFromParent();
  return;
 }
 if(auto* Owner=TerminalOwner.Get())if(auto* Close=Owner->FindFunction(TEXT("Close")))Owner->ProcessEvent(Close,nullptr);
}
void UVTGTerminalSavePage::TerminalVisibilityChanged(ESlateVisibility InVisibility)
{if(InVisibility==ESlateVisibility::Collapsed||InVisibility==ESlateVisibility::Hidden){PendingAction=0;SetVisibility(ESlateVisibility::Collapsed);}}
void UVTGTerminalSavePage::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);
 // Combat can start or end while the page is open, but a fight does not begin and end inside a frame
 // - five checks a second is plenty, and it keeps a reflected property read off the per-frame path.
 AvailabilityCountdown-=DeltaTime;
 if(AvailabilityCountdown<=0.f){AvailabilityCountdown=0.2f;RefreshAvailability();}
 // The 3D prop may be hidden while the 2D inventory terminal is open.
 // Visibility is owned exclusively by the terminal widget and its Close action.
}
void UVTGTerminalSavePage::RefreshTerminalFrame()
{
 if(!HasTerminalFrame||!WidgetTree)return;
 auto* Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget);if(!Canvas)return;
 if(SharedBackgroundClass)
 {
  if(!SharedBackground)
  {
   SharedBackground=CreateWidget<UUserWidget>(this,SharedBackgroundClass,TEXT("TerminalSharedBackground"));
   auto* BackgroundSlot=Canvas->AddChildToCanvas(SharedBackground);BackgroundSlot->SetAnchors(FAnchors(0,0,1,1));BackgroundSlot->SetOffsets(FMargin(0));BackgroundSlot->SetZOrder(-3);
   SharedBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
  }
  // Use the entire authored Image_40 -> BackgroundBlur -> Terminal hierarchy.
  // This preserves its backdrop and blur composition instead of approximating it.
 }
 else
 {
 if(BlurSource)
 {
  auto* Blur=Cast<UBackgroundBlur>(GetWidgetFromName(TEXT("TerminalBackgroundBlur")));
  if(!Blur){Blur=WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(),TEXT("TerminalBackgroundBlur"));Canvas->AddChildToCanvas(Blur);}
  Blur->SetBlurStrength(BlurSource->GetBlurStrength());Blur->SetBlurRadius(BlurSource->GetBlurRadius());Blur->SetOverrideAutoRadiusCalculation(BlurSource->GetOverrideAutoRadiusCalculation());
  Blur->SetApplyAlphaToBlur(BlurSource->GetApplyAlphaToBlur());Blur->SetCornerRadius(BlurSource->GetCornerRadius());Blur->SetLowQualityFallbackBrush(BlurSource->GetLowQualityFallbackBrush());
  Blur->SetVisibility(ESlateVisibility::HitTestInvisible);auto* BlurSlot=CastChecked<UCanvasPanelSlot>(Blur->Slot);BlurSlot->SetLayout(BlurLayout);BlurSlot->SetZOrder(-3);
 }
 auto* Frame=Cast<UImage>(GetWidgetFromName(TEXT("TerminalFrame")));
 if(!Frame){Frame=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("TerminalFrame"));Canvas->AddChildToCanvas(Frame);}
 Frame->SetBrush(TerminalFrameBrush);Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
 auto* FrameSlot=CastChecked<UCanvasPanelSlot>(Frame->Slot);FrameSlot->SetLayout(TerminalFrameLayout);FrameSlot->SetZOrder(-2);
 }
 if(HasTerminalArtwork)
 {
  auto* Artwork=Cast<UImage>(GetWidgetFromName(TEXT("TerminalArtwork")));
  if(!Artwork){Artwork=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("TerminalArtwork"));Canvas->AddChildToCanvas(Artwork);}
  Artwork->SetBrush(TerminalArtworkBrush);Artwork->SetVisibility(ESlateVisibility::HitTestInvisible);auto* S=CastChecked<UCanvasPanelSlot>(Artwork->Slot);S->SetLayout(TerminalArtworkLayout);S->SetZOrder(-1);
 }
}
void UVTGTerminalSavePage::NativeDestruct()
{if(auto* Owner=TerminalOwner.Get())Owner->OnVisibilityChanged.RemoveDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);Super::NativeDestruct();}
