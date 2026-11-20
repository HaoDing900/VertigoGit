#include "UI/VTGPauseMenu.h"
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
 auto* Continue=Cast<UButton>(GetWidgetFromName(TEXT("B_Continue")));
 auto* Exit=Cast<UButton>(GetWidgetFromName(TEXT("B_Exit")));
 auto* List=Cast<UVerticalBox>(GetWidgetFromName(TEXT("VerticalBox_137")));
 if(!Continue || !Exit || !List)return;
 if(!LanguageButton)
 {
  auto* Ref=Cast<UTextBlock>(Continue->GetContent());if(!Ref)return;
  LanguageButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("B_Language"));
  LanguageButton->SetStyle(Continue->GetStyle());LanguageButton->SetColorAndOpacity(Continue->GetColorAndOpacity());LanguageButton->SetBackgroundColor(Continue->GetBackgroundColor());
  LanguageLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("LanguageLabel"));
  LanguageLabel->SetFont(Ref->GetFont());LanguageLabel->SetColorAndOpacity(Ref->GetColorAndOpacity());LanguageLabel->SetShadowColorAndOpacity(Ref->GetShadowColorAndOpacity());LanguageLabel->SetShadowOffset(Ref->GetShadowOffset());LanguageLabel->SetJustification(ETextJustify::Left);
  LanguageButton->SetContent(LanguageLabel);
  auto* RefSlot=CastChecked<UButtonSlot>(Ref->Slot);auto* TextSlot=CastChecked<UButtonSlot>(LanguageLabel->Slot);
  TextSlot->SetPadding(RefSlot->GetPadding());TextSlot->SetHorizontalAlignment(RefSlot->GetHorizontalAlignment());TextSlot->SetVerticalAlignment(RefSlot->GetVerticalAlignment());
  auto* OldExit=CastChecked<UVerticalBoxSlot>(Exit->Slot);const auto ButtonPadding=OldExit->GetPadding();const auto Size=OldExit->GetSize();const auto Horizontal=OldExit->GetHorizontalAlignment();const auto Vertical=OldExit->GetVerticalAlignment();
  List->RemoveChild(Exit);
  for(auto* Button:{LanguageButton.Get(),Exit}){auto* S=List->AddChildToVerticalBox(Button);S->SetPadding(ButtonPadding);S->SetSize(Size);S->SetHorizontalAlignment(Horizontal);S->SetVerticalAlignment(Vertical);}
  Continue->SetNavigationRuleExplicit(EUINavigation::Down,LanguageButton);LanguageButton->SetNavigationRuleExplicit(EUINavigation::Up,Continue);LanguageButton->SetNavigationRuleExplicit(EUINavigation::Down,Exit);Exit->SetNavigationRuleExplicit(EUINavigation::Up,LanguageButton);
 }
 LanguageButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleLanguage);
 Languages=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGLocalizationSubsystem>():nullptr;
 LanguageButton->SetIsEnabled(Languages!=nullptr);
 if(Languages){Languages->OnLanguageChanged.AddUniqueDynamic(this,&ThisClass::RefreshLanguage);RefreshLanguage(Languages->GetLanguage());}
 else RefreshLanguage(TEXT("en-US"));
}
void UVTGPauseMenu::ToggleLanguage()
{
 if(!Languages)return;
 FString Next=Languages->GetLanguage()==TEXT("zh-Hans")?TEXT("en-US"):TEXT("zh-Hans");
#if WITH_EDITOR
 if(Languages->GetLanguage()==TEXT("en-US"))Next=TEXT("origin");
#endif
 if(Languages->SetLanguage(Next))RefreshLanguage(Languages->GetLanguage());
}
void UVTGPauseMenu::RefreshLanguage(const FString& Language)
{
 if(!LanguageLabel)return;
 // Explicit language names remain legible even when content translations are incomplete.
 LanguageLabel->SetText(FText::AsCultureInvariant(Language==TEXT("origin")?TEXT("Language: Origin (Dev)"):Language==TEXT("zh-Hans")?TEXT("语言：简体中文"):TEXT("Language: English")));
 LanguageButton->SetToolTipText(FText::AsCultureInvariant(Language==TEXT("zh-Hans")?TEXT("点击切换为英文"):TEXT("Switch to Simplified Chinese")));
}
void UVTGPauseMenu::NativeDestruct()
{
 if(Languages)Languages->OnLanguageChanged.RemoveDynamic(this,&ThisClass::RefreshLanguage);
 if(LanguageButton)LanguageButton->OnClicked.RemoveDynamic(this,&ThisClass::ToggleLanguage);
 Languages=nullptr;Super::NativeDestruct();
}
