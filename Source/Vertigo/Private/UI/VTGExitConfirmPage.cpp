#include "UI/VTGExitConfirmPage.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"

namespace VTGExitPage
{
FSlateFontInfo Font(const TCHAR* Weight, int32 Size)
{
 return FSlateFontInfo(LoadObject<UFont>(nullptr, *(FString(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-")) + Weight + TEXT("_Font"))), Size);
}
FText Text(const TCHAR* Value) { return FText::AsCultureInvariant(Value); }
/** Matches the language page: one switch per string, driven by the localization subsystem. */
FText Pick(bool bChinese, const TCHAR* English, const TCHAR* Chinese) { return Text(bChinese ? Chinese : English); }
const FLinearColor Ink(.90f, .95f, .95f, 1.f);
const FLinearColor Accent(.55f, .73f, .76f, 1.f);
}

UVTGExitConfirmPage::UVTGExitConfirmPage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 static ConstructorHelpers::FObjectFinder<UFont> Regular(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));
 static ConstructorHelpers::FObjectFinder<UFont> Medium(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));
 static ConstructorHelpers::FObjectFinder<UFont> Semibold(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-SemiBold_Font"));
 FontAssets={Regular.Object,Medium.Object,Semibold.Object};
}

void UVTGExitConfirmPage::Configure(UVTGLocalizationSubsystem* InLanguages, UTexture2D* Frame, TSoftObjectPtr<UWorld> InMainMenuLevel)
{
 Languages=InLanguages;
 MainMenuLevel=InMainMenuLevel;
 if(!WidgetTree->RootWidget)BuildPage(Frame);
 RefreshLanguage(Languages?Languages->GetLanguage():TEXT("en-US"));
}

void UVTGExitConfirmPage::BuildPage(UTexture2D* Frame)
{
 using namespace VTGExitPage;
 auto* Root=WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("ExitScale"));
 Root->SetStretch(EStretch::ScaleToFit);
 Root->SetStretchDirection(EStretchDirection::DownOnly);
 WidgetTree->RootWidget=Root;
 auto* Size=WidgetTree->ConstructWidget<USizeBox>();
 Size->SetWidthOverride(940);Size->SetHeightOverride(800);
 Root->SetContent(Size);
 auto* ScaleSlot=CastChecked<UScaleBoxSlot>(Size->Slot);
 ScaleSlot->SetHorizontalAlignment(HAlign_Center);ScaleSlot->SetVerticalAlignment(VAlign_Center);
 auto* Panel=WidgetTree->ConstructWidget<UCanvasPanel>();Size->SetContent(Panel);
 auto Place=[Panel](UWidget* Child,FVector2D Position,FVector2D Dimensions)
 {
  auto* Slot=Panel->AddChildToCanvas(Child);Slot->SetPosition(Position);Slot->SetSize(Dimensions);
 };
 auto* Art=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("ExitFrame"));
 Art->SetBrushFromTexture(Frame);Art->SetVisibility(ESlateVisibility::HitTestInvisible);
 Place(Art,FVector2D::ZeroVector,FVector2D(940,800));
 auto MakeText=[&](const TCHAR* Name,const TCHAR* Value,int32 SizeValue,const TCHAR* Weight,FVector2D Position,FVector2D Dimensions)
 {
  auto* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  Label->SetText(Text(Value));Label->SetFont(Font(Weight,SizeValue));
  Label->SetColorAndOpacity(FSlateColor(Ink));
  Label->SetAutoWrapText(true);Place(Label,Position,Dimensions);return Label;
 };
 auto* Breadcrumb=MakeText(TEXT("ExitBreadcrumb"),TEXT("PAUSE / EXIT"),16,TEXT("Medium"),FVector2D(100,94),FVector2D(740,30));
 Breadcrumb->SetColorAndOpacity(FSlateColor(Accent));
 Heading=MakeText(TEXT("ExitHeading"),TEXT("Exit Game"),36,TEXT("SemiBold"),FVector2D(100,142),FVector2D(740,70));
 auto* Line=WidgetTree->ConstructWidget<UBorder>();
 Line->SetBrushColor(FLinearColor(.36f,.65f,.68f,.55f));Line->SetVisibility(ESlateVisibility::HitTestInvisible);
 Place(Line,FVector2D(100,235),FVector2D(740,1));
 Question=MakeText(TEXT("ExitQuestion"),TEXT("Where do you want to go?"),24,TEXT("Medium"),FVector2D(100,280),FVector2D(740,46));

 // Share the language page's flat button treatment so the two cards read as one screen.
 FButtonStyle Choice;
 Choice.SetNormal(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.08f)));
 Choice.SetHovered(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.20f)));
 Choice.SetPressed(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.30f)));
 Choice.SetDisabled(FSlateNoResource());
 auto MakeChoice=[&](const TCHAR* Name,const TCHAR* LabelName,float Y,UTextBlock*& OutLabel)
 {
  auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
  Button->SetStyle(Choice);
  OutLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),LabelName);
  OutLabel->SetFont(Font(TEXT("Medium"),24));OutLabel->SetColorAndOpacity(FSlateColor(Ink));
  OutLabel->SetJustification(ETextJustify::Center);
  Button->SetContent(OutLabel);
  CastChecked<UButtonSlot>(OutLabel->Slot)->SetPadding(FMargin(16,12));
  Place(Button,FVector2D(100,Y),FVector2D(740,68));
  return Button;
 };
 UTextBlock* MainMenuText=nullptr;
 UTextBlock* DesktopText=nullptr;
 MainMenuButton=MakeChoice(TEXT("ExitToMainMenu"),TEXT("ExitToMainMenuLabel"),340,MainMenuText);
 DesktopButton=MakeChoice(TEXT("ExitToDesktop"),TEXT("ExitToDesktopLabel"),426,DesktopText);
 MainMenuLabel=MainMenuText;DesktopLabel=DesktopText;
 MainMenuButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ReturnToMainMenu);
 DesktopButton->OnClicked.AddUniqueDynamic(this,&ThisClass::QuitToDesktop);

 WarningLabel=MakeText(TEXT("ExitWarning"),TEXT(""),18,TEXT("Regular"),FVector2D(100,520),FVector2D(740,76));
 WarningLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f,.59f,.42f,1)));

 BackButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ExitBack"));
 FButtonStyle BackStyle;
 BackStyle.SetNormal(FSlateNoResource());
 BackStyle.SetDisabled(FSlateNoResource());
 BackStyle.SetHovered(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.15f)));
 BackStyle.SetPressed(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.24f)));
 BackButton->SetStyle(BackStyle);
 BackLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ExitBackLabel"));
 BackLabel->SetFont(Font(TEXT("Medium"),22));BackLabel->SetColorAndOpacity(FSlateColor(Ink));
 BackLabel->SetJustification(ETextJustify::Center);BackButton->SetContent(BackLabel);
 BackButton->OnClicked.AddUniqueDynamic(this,&ThisClass::RequestBack);
 Place(BackButton,FVector2D(100,626),FVector2D(200,62));
 MakeText(TEXT("ExitBackHint"),TEXT("Esc / B"),16,TEXT("Regular"),FVector2D(710,646),FVector2D(130,32))->SetJustification(ETextJustify::Right);

 // Back is the safe default, so arrive on it rather than on either way of ending the session.
 MainMenuButton->SetNavigationRuleExplicit(EUINavigation::Down,DesktopButton);
 MainMenuButton->SetNavigationRuleExplicit(EUINavigation::Up,BackButton);
 DesktopButton->SetNavigationRuleExplicit(EUINavigation::Up,MainMenuButton);
 DesktopButton->SetNavigationRuleExplicit(EUINavigation::Down,BackButton);
 BackButton->SetNavigationRuleExplicit(EUINavigation::Up,DesktopButton);
 BackButton->SetNavigationRuleExplicit(EUINavigation::Down,MainMenuButton);
}

void UVTGExitConfirmPage::NativeConstruct()
{
 Super::NativeConstruct();
 if(Languages)
 {
  Languages->OnLanguageChanged.AddUniqueDynamic(this,&ThisClass::RefreshLanguage);
  RefreshLanguage(Languages->GetLanguage());
 }
}

void UVTGExitConfirmPage::NativeDestruct()
{
 if(Languages)Languages->OnLanguageChanged.RemoveDynamic(this,&ThisClass::RefreshLanguage);
 Super::NativeDestruct();
}

void UVTGExitConfirmPage::RefreshLanguage(const FString& Language)
{
 using namespace VTGExitPage;
 if(!Heading)return;
 const bool Chinese=Language==TEXT("zh-Hans");
 Heading->SetText(Pick(Chinese,TEXT("Exit Game"),TEXT("退出游戏")));
 Question->SetText(Pick(Chinese,TEXT("Where do you want to go?"),TEXT("要去哪里？")));
 MainMenuLabel->SetText(Pick(Chinese,TEXT("Return to Main Menu"),TEXT("返回主菜单")));
 DesktopLabel->SetText(Pick(Chinese,TEXT("Quit to Desktop"),TEXT("退出到桌面")));
 BackLabel->SetText(Pick(Chinese,TEXT("Back"),TEXT("返回")));
 WarningLabel->SetText(Pick(Chinese,TEXT("Progress since your last save will be lost."),TEXT("上次存档之后的进度将会丢失。")));
 MainMenuButton->SetToolTipText(MainMenuLabel->GetText());
 DesktopButton->SetToolTipText(DesktopLabel->GetText());
}

void UVTGExitConfirmPage::FocusSelection() { if(BackButton)BackButton->SetKeyboardFocus(); }

void UVTGExitConfirmPage::RequestBack() { OnBackRequested.Broadcast(); }

void UVTGExitConfirmPage::ReleasePause()
{
 UGameplayStatics::SetGamePaused(this,false);
 if(APlayerController* PC=GetOwningPlayer())
 {
  PC->SetInputMode(FInputModeGameOnly());
  PC->SetShowMouseCursor(false);
 }
}

void UVTGExitConfirmPage::ReturnToMainMenu()
{
 if(bLeaving || MainMenuLevel.IsNull())return;
 bLeaving=true;
 ReleasePause();
 // Same route the death screen takes, so both ways back to the menu share one loading screen.
 UVTGMediaLoadingPageSystem::OpenLevelWithLoadingScreen(this,MainMenuLevel);
}

void UVTGExitConfirmPage::QuitToDesktop()
{
 if(bLeaving)return;
 bLeaving=true;
 ReleasePause();
 UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
}
