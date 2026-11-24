#include "UI/VTGPauseMenu.h"
#include "UI/VTGLanguageSettingsPage.h"
#include "UI/VTGExitConfirmPage.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "InputCoreTypes.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

void UVTGPauseMenu::NativeConstruct()
{
 Super::NativeConstruct();
 SetIsFocusable(true);
 auto* Continue=Cast<UButton>(GetWidgetFromName(TEXT("B_Continue")));
 auto* Exit=Cast<UButton>(GetWidgetFromName(TEXT("B_Exit")));
 auto* List=Cast<UVerticalBox>(GetWidgetFromName(TEXT("VerticalBox_137")));
 if(!Continue || !Exit || !List)return;
 // Prefer the authored button, so Designer and runtime share one layout/style.
 if(!LanguageButton)LanguageButton=Cast<UButton>(GetWidgetFromName(TEXT("B_Language")));
 if(LanguageButton)LanguageLabel=Cast<UTextBlock>(LanguageButton->GetContent());
 if(!LanguageButton)
 {
  auto* Ref=Cast<UTextBlock>(Continue->GetContent());if(!Ref)return;
  LanguageButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("B_Language"));
  LanguageButton->SetStyle(Continue->GetStyle());LanguageButton->SetColorAndOpacity(Continue->GetColorAndOpacity());LanguageButton->SetBackgroundColor(Continue->GetBackgroundColor());
  LanguageLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("LanguageLabel"));
  LanguageLabel->SetText(NSLOCTEXT("VTGPauseMenu", "Language", "Language"));
  LanguageLabel->SetFont(Ref->GetFont());LanguageLabel->SetColorAndOpacity(Ref->GetColorAndOpacity());LanguageLabel->SetShadowColorAndOpacity(Ref->GetShadowColorAndOpacity());LanguageLabel->SetShadowOffset(Ref->GetShadowOffset());LanguageLabel->SetJustification(ETextJustify::Left);
  LanguageButton->SetContent(LanguageLabel);
  auto* RefSlot=CastChecked<UButtonSlot>(Ref->Slot);auto* TextSlot=CastChecked<UButtonSlot>(LanguageLabel->Slot);
  TextSlot->SetPadding(RefSlot->GetPadding());TextSlot->SetHorizontalAlignment(RefSlot->GetHorizontalAlignment());TextSlot->SetVerticalAlignment(RefSlot->GetVerticalAlignment());
  auto* OldExit=CastChecked<UVerticalBoxSlot>(Exit->Slot);const auto ButtonPadding=OldExit->GetPadding();const auto Size=OldExit->GetSize();const auto Horizontal=OldExit->GetHorizontalAlignment();const auto Vertical=OldExit->GetVerticalAlignment();
  List->RemoveChild(Exit);
  for(auto* Button:{LanguageButton.Get(),Exit}){auto* S=List->AddChildToVerticalBox(Button);S->SetPadding(ButtonPadding);S->SetSize(Size);S->SetHorizontalAlignment(Horizontal);S->SetVerticalAlignment(Vertical);}
  Continue->SetNavigationRuleExplicit(EUINavigation::Down,LanguageButton);LanguageButton->SetNavigationRuleExplicit(EUINavigation::Up,Continue);LanguageButton->SetNavigationRuleExplicit(EUINavigation::Down,Exit);Exit->SetNavigationRuleExplicit(EUINavigation::Up,LanguageButton);
 }
 LanguageButton->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenLanguagePage);
 // The Blueprint On Clicked handler that called Quit Game directly is gone; Exit now asks where to.
 ExitButton=Exit;
 Exit->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenExitPage);
 Continue->SetNavigationRuleExplicit(EUINavigation::Down,LanguageButton);
 LanguageButton->SetNavigationRuleExplicit(EUINavigation::Up,Continue);
 LanguageButton->SetNavigationRuleExplicit(EUINavigation::Down,Exit);
 Exit->SetNavigationRuleExplicit(EUINavigation::Up,LanguageButton);
 Languages=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGLocalizationSubsystem>():nullptr;
 LanguageButton->SetIsEnabled(Languages!=nullptr);
 if(Languages){Languages->OnLanguageChanged.AddUniqueDynamic(this,&ThisClass::RefreshLanguage);RefreshLanguage(Languages->GetLanguage());}
 else RefreshLanguage(TEXT("en-US"));
 if(LanguagePage)LanguagePage->OnBackRequested.AddUniqueDynamic(this,&ThisClass::ShowMainPage);
 ShowMainPage();
}
UCanvasPanel* UVTGPauseMenu::PageCanvas()
{
 auto* Canvas=Cast<UCanvasPanel>(GetWidgetFromName(TEXT("CanvasPanel_29")));
 if(!Canvas)Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget);
 ensureMsgf(Canvas,TEXT("Pause menu requires its content canvas"));
 return Canvas;
}
void UVTGPauseMenu::OpenPage(UUserWidget* Page)
{
 auto* Canvas=PageCanvas();
 if(!Canvas)return;
 auto* PageSlot=Canvas->AddChildToCanvas(Page);
 PageSlot->SetAnchors(FAnchors(0,0,1,1));
 // Leave the authored pause heading visible above the overlaid settings card.
 float TopInset=40.f;
 if(auto* Heading=GetWidgetFromName(TEXT("TextBlock_81")))
  if(auto* HeadingSlot=Cast<UCanvasPanelSlot>(Heading->Slot))
   TopInset=FMath::Max(TopInset,HeadingSlot->GetPosition().Y+HeadingSlot->GetSize().Y+24.f);
 PageSlot->SetOffsets(FMargin(40,TopInset,40,40));PageSlot->SetZOrder(10);
}
void UVTGPauseMenu::OpenLanguagePage()
{
 if(!Languages)return;
 if(!LanguagePage)
 {
  if(!PageCanvas())return;
  LanguagePage=CreateWidget<UVTGLanguageSettingsPage>(this,UVTGLanguageSettingsPage::StaticClass());
  if(!LanguagePage)return;
  LanguagePage->Configure(Languages,SettingsFrame);
  LanguagePage->OnBackRequested.AddUniqueDynamic(this,&ThisClass::ShowMainPage);
  OpenPage(LanguagePage);
 }
 bSettingsOpen=true;
 // Keep the Pause WBP intact underneath; disable its buttons while the card owns input.
 if(auto* List=GetWidgetFromName(TEXT("VerticalBox_137")))List->SetIsEnabled(false);
 LanguagePage->SetVisibility(ESlateVisibility::Visible);LanguagePage->FocusSelection();
}
void UVTGPauseMenu::OpenExitPage()
{
 if(!ExitPage)
 {
  if(!PageCanvas())return;
  ExitPage=CreateWidget<UVTGExitConfirmPage>(this,UVTGExitConfirmPage::StaticClass());
  if(!ExitPage)return;
  ExitPage->Configure(Languages,SettingsFrame,MainMenuLevel);
  ExitPage->OnBackRequested.AddUniqueDynamic(this,&ThisClass::ShowMainPage);
  OpenPage(ExitPage);
 }
 bSettingsOpen=true;
 if(auto* List=GetWidgetFromName(TEXT("VerticalBox_137")))List->SetIsEnabled(false);
 ExitPage->SetVisibility(ESlateVisibility::Visible);ExitPage->FocusSelection();
}
void UVTGPauseMenu::ShowMainPage()
{
 const bool RestoreFocus=bSettingsOpen;bSettingsOpen=false;
 if(LanguagePage){LanguagePage->CloseDropdown();LanguagePage->SetVisibility(ESlateVisibility::Collapsed);}
 if(ExitPage)ExitPage->SetVisibility(ESlateVisibility::Collapsed);
 if(RestoreFocus)
  if(auto* List=GetWidgetFromName(TEXT("VerticalBox_137")))List->SetIsEnabled(true);
 if(RestoreFocus && LanguageButton)LanguageButton->SetKeyboardFocus();
}
bool UVTGPauseMenu::HandleSettingsBack()
{
 if(!bSettingsOpen)return false;
 // Only the language card has a popup that Back must close before it leaves the page.
 if(LanguagePage && LanguagePage->GetVisibility()!=ESlateVisibility::Collapsed && LanguagePage->CloseDropdown())return true;
 ShowMainPage();return true;
}
FReply UVTGPauseMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right)
 {
  if(Event.IsRepeat())return FReply::Handled();
  if(HandleSettingsBack())return FReply::Handled();
  if(auto* Continue=Cast<UButton>(GetWidgetFromName(TEXT("B_Continue"))))
  {
   // Preserve the Blueprint resume handler's input and cursor cleanup.
   Continue->OnPressed.Broadcast();return FReply::Handled();
  }
 }
 return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
void UVTGPauseMenu::RefreshLanguage(const FString& Language)
{
 if(!LanguageLabel)return;
 // Keep the authored FText identity: Slate refreshes it when localization resources change.
 LanguageButton->SetToolTipText(FText::AsCultureInvariant(Language==TEXT("zh-Hans")?TEXT("打开语言设置"):TEXT("Open language settings")));
}
void UVTGPauseMenu::NativeDestruct()
{
 if(Languages)Languages->OnLanguageChanged.RemoveDynamic(this,&ThisClass::RefreshLanguage);
 if(LanguageButton)LanguageButton->OnClicked.RemoveDynamic(this,&ThisClass::OpenLanguagePage);
 if(LanguagePage){LanguagePage->CloseDropdown();LanguagePage->OnBackRequested.RemoveDynamic(this,&ThisClass::ShowMainPage);}
 if(ExitPage)ExitPage->OnBackRequested.RemoveDynamic(this,&ThisClass::ShowMainPage);
 if(ExitButton)ExitButton->OnClicked.RemoveDynamic(this,&ThisClass::OpenExitPage);
 bSettingsOpen=false;
 Languages=nullptr;Super::NativeDestruct();
}
