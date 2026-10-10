#include "UI/VTGTerminalSavePage.h"
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

void UVTGTerminalSlotButton::Setup(UVTGTerminalSavePage* Page,int32 Index)
{ PageOwner=Page;SlotIndex=Index;IsFocusable=false;OnClicked.AddUniqueDynamic(this,&UVTGTerminalSlotButton::Select); }
void UVTGTerminalSlotButton::Select(){if(PageOwner)PageOwner->SelectSlot(SlotIndex);}

UTextBlock* UVTGTerminalSavePage::Text(FName Name,const FText& Value,int32 Size)
{
 auto* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
 Label->SetText(Value);auto Font=Label->GetFont();Font.Size=Size;Label->SetFont(Font);
 Label->SetColorAndOpacity(FSlateColor(FLinearColor(.8f,.95f,.94f)));Label->SetAutoWrapText(true);return Label;
}
UButton* UVTGTerminalSavePage::Action(FName Name,const FText& Label)
{
 auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);Button->IsFocusable=false;
 FButtonStyle Style=Button->GetStyle();Style.Normal.TintColor=FSlateColor(FLinearColor(.06f,.25f,.27f));
 Style.Hovered.TintColor=FSlateColor(FLinearColor(.12f,.46f,.48f));Style.Pressed.TintColor=FSlateColor(FLinearColor(.04f,.17f,.18f));
 Style.NormalPadding=FMargin(20,12);Style.PressedPadding=FMargin(20,14,20,10);Button->SetStyle(Style);
 auto* Caption=Text(NAME_None,Label,20);Caption->SetJustification(ETextJustify::Center);Button->SetContent(Caption);return Button;
}
TSharedRef<SWidget> UVTGTerminalSavePage::RebuildWidget()
{
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
 if(!WidgetTree->RootWidget)
 {
  auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
  Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  auto* Screen=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("TerminalScreen"));
  Screen->SetBrushColor(FLinearColor(.012f,.035f,.04f,.96f));Screen->SetPadding(FMargin(28));Screen->SetClipping(EWidgetClipping::ClipToBoundsAlways);
  auto* ScreenSlot=Canvas->AddChildToCanvas(Screen);
  if(!HasScreenLayout){ScreenLayout.Anchors=FAnchors(.5f,.5f);ScreenLayout.Offsets=FMargin(-580,-340,1152,681.5f);}
  ScreenSlot->SetLayout(ScreenLayout);
  auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Screen->SetContent(Body);
  Body->AddChildToVerticalBox(Text(TEXT("SaveHeading"),NSLOCTEXT("VTGTerminal","SaveLoad","SAVE / LOAD"),28))->SetPadding(FMargin(0,0,0,8));
  Body->AddChildToVerticalBox(Text(NAME_None,NSLOCTEXT("VTGTerminal","SlotHelp","9 manual slots. Automatic checkpoints are protected."),17))->SetPadding(FMargin(0,0,0,18));
  auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();auto* MainSlot=Body->AddChildToVerticalBox(Columns);MainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  SlotList=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("SaveSlots"));SlotList->SetClipping(EWidgetClipping::ClipToBoundsAlways);SlotList->SetAlwaysShowScrollbar(true);
  auto* ListSlot=Columns->AddChildToHorizontalBox(SlotList);ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));ListSlot->SetPadding(FMargin(0,0,24,0));
  auto* Details=WidgetTree->ConstructWidget<UBorder>();Details->SetBrushColor(FLinearColor(.025f,.08f,.09f,.9f));Details->SetPadding(FMargin(24));
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
void UVTGTerminalSavePage::OpenForTerminal(UUserWidget* Terminal,UUserWidget* Source)
{
 if(auto* Previous=TerminalOwner.Get())Previous->OnVisibilityChanged.RemoveDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);
 TerminalOwner=Terminal;
 if(Terminal)Terminal->OnVisibilityChanged.AddUniqueDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);
 // Use the actual mail content rectangle, not the full-screen widget switcher bounds.
  if(Source){
  if(auto* Background=Cast<UUserWidget>(Source->GetWidgetFromName(TEXT("WB_Background"))))
   if(auto* FrameImage=Cast<UImage>(Background->GetWidgetFromName(TEXT("Terminal"))))
    if(auto* FrameImageSlot=Cast<UCanvasPanelSlot>(FrameImage->Slot)){
     TerminalFrameBrush=FrameImage->GetBrush();TerminalFrameLayout=FrameImageSlot->GetLayout();HasTerminalFrame=true;
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
  FString Title=M.SlotIndex==0?TEXT("AUTO SAVE"):FString::Printf(TEXT("MANUAL %02d"),M.SlotIndex);
  FString Summary=M.bIsValid?FPackageName::GetShortName(M.LevelName).Replace(TEXT("_"),TEXT(" ")):TEXT("Empty slot");
  if(M.bIsValid)Summary+=TEXT("  |  ")+FText::AsDateTime(M.SaveTimeUtc,EDateTimeStyle::Short,EDateTimeStyle::Short).ToString();
  Row->SetContent(Text(NAME_None,FText::FromString(Title+TEXT("\n")+Summary),17));CastChecked<UButtonSlot>(Row->GetContent()->Slot)->SetHorizontalAlignment(HAlign_Fill);
  FButtonStyle Style=Row->GetStyle();Style.NormalPadding=FMargin(16,12);Style.PressedPadding=FMargin(16,14,16,10);Row->SetStyle(Style);
  SlotList->AddChild(Row);SlotButtons.Add(Row);
 }
 UpdateSelection();
}
void UVTGTerminalSavePage::SelectSlot(int32 Index){if(Busy)return;SelectedSlot=Index;PendingAction=0;UpdateSelection();}
void UVTGTerminalSavePage::UpdateSelection()
{
 if(!SaveButton)return;
 const auto* M=Metas.FindByPredicate([&](const FVTGSlotMeta& Value){return Value.SlotIndex==SelectedSlot;});
 for(int32 I=0;I<SlotButtons.Num();++I){auto* B=SlotButtons[I].Get();auto Style=B->GetStyle();Style.Normal.TintColor=FSlateColor(Metas[I].SlotIndex==SelectedSlot?FLinearColor(.1f,.4f,.41f):FLinearColor(.035f,.12f,.14f));Style.Hovered.TintColor=FSlateColor(FLinearColor(.12f,.46f,.48f));B->SetStyle(Style);}
 FString Info=SelectedSlot==0?TEXT("AUTO SAVE\n\nRead-only checkpoint"):FString::Printf(TEXT("MANUAL SAVE %02d"),SelectedSlot);
 if(M&&M->bIsValid)Info+=TEXT("\n\n")+FPackageName::GetShortName(M->LevelName).Replace(TEXT("_"),TEXT(" "))+TEXT("\n\n")+FText::AsDateTime(M->SaveTimeUtc,EDateTimeStyle::Medium,EDateTimeStyle::Short).ToString()+TEXT("\n\n")+M->DisplayLabel;
 else Info+=TEXT("\n\nEmpty slot\n\nSave your current position and progress here.");
 DetailText->SetText(FText::FromString(Info));
 SaveButton->SetIsEnabled(M&&SelectedSlot>0&&!Busy&&PendingAction!=2);LoadButton->SetIsEnabled(M&&M->bIsValid&&!Busy&&PendingAction!=1);
 CastChecked<UTextBlock>(SaveButton->GetContent())->SetText(PendingAction==1?NSLOCTEXT("VTGTerminal","Overwrite","CONFIRM SAVE"):NSLOCTEXT("VTGTerminal","SaveButton","SAVE GAME"));
 CastChecked<UTextBlock>(LoadButton->GetContent())->SetText(PendingAction==2?NSLOCTEXT("VTGTerminal","ConfirmLoad","CONFIRM LOAD"):NSLOCTEXT("VTGTerminal","LoadButton","LOAD GAME"));
 CancelButton->SetVisibility(PendingAction?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(!PendingAction){StatusText->SetText(NSLOCTEXT("VTGTerminal","ChooseSlot","Select a slot to save or load."));StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(.8f,.95f,.94f)));}
}
void UVTGTerminalSavePage::ShowResult(const FText& Message,bool Success)
{StatusText->SetText(Message);StatusText->SetColorAndOpacity(FSlateColor(Success?FLinearColor(.45f,1.f,.65f):FLinearColor(1.f,.65f,.4f)));}
void UVTGTerminalSavePage::SaveClicked()
{
 if(Busy||SelectedSlot<=0)return;
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
 }else{Busy=false;PendingAction=0;UpdateSelection();ShowResult(NSLOCTEXT("VTGTerminal","LoadFailed","Could not load this save."),false);}
}
void UVTGTerminalSavePage::CancelClicked(){PendingAction=0;UpdateSelection();}
void UVTGTerminalSavePage::CloseClicked()
{
 PendingAction=0;SetVisibility(ESlateVisibility::Collapsed);
 if(auto* Owner=TerminalOwner.Get())if(auto* Close=Owner->FindFunction(TEXT("Close")))Owner->ProcessEvent(Close,nullptr);
}
void UVTGTerminalSavePage::TerminalVisibilityChanged(ESlateVisibility InVisibility)
{if(InVisibility==ESlateVisibility::Collapsed||InVisibility==ESlateVisibility::Hidden){PendingAction=0;SetVisibility(ESlateVisibility::Collapsed);}}
void UVTGTerminalSavePage::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);
 // The 3D prop may be hidden while the 2D inventory terminal is open.
 // Visibility is owned exclusively by the terminal widget and its Close action.
}
void UVTGTerminalSavePage::RefreshTerminalFrame()
{
 if(!HasTerminalFrame||!WidgetTree)return;
 auto* Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget);if(!Canvas)return;
 auto* Frame=Cast<UImage>(GetWidgetFromName(TEXT("TerminalFrame")));
 if(!Frame){Frame=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("TerminalFrame"));Canvas->AddChildToCanvas(Frame);}
 Frame->SetBrush(TerminalFrameBrush);Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
 auto* FrameSlot=CastChecked<UCanvasPanelSlot>(Frame->Slot);FrameSlot->SetLayout(TerminalFrameLayout);FrameSlot->SetZOrder(-1);
}
void UVTGTerminalSavePage::NativeDestruct()
{if(auto* Owner=TerminalOwner.Get())Owner->OnVisibilityChanged.RemoveDynamic(this,&UVTGTerminalSavePage::TerminalVisibilityChanged);Super::NativeDestruct();}
