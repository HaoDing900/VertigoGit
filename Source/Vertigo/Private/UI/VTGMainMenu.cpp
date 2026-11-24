#include "UI/VTGMainMenu.h"
#include "Save/VTGSaveCoordinator.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "UI/VTGTerminalSavePage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"

void UVTGMainMenu::NativeConstruct()
{
 Super::NativeConstruct();
 if(auto* Languages=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGLocalizationSubsystem>():nullptr)
 {
  auto* Combo=Cast<UComboBoxString>(GetWidgetFromName(TEXT("TextLanguageSelector")));
  if(!Combo)if(auto* Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget))
  {
   Combo=WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),TEXT("TextLanguageSelector"));
   auto* LanguageSlot=Canvas->AddChildToCanvas(Combo);LanguageSlot->SetAnchors(FAnchors(1,1));LanguageSlot->SetAlignment(FVector2D(1,1));LanguageSlot->SetPosition(FVector2D(-40,-40));LanguageSlot->SetSize(FVector2D(220,40));LanguageSlot->SetZOrder(20);
  }
  if(Combo)
  {
   Combo->ClearOptions();
   const auto Codes=Languages->GetLanguages();
   for(const auto& Language:Codes)Combo->AddOption(Language==TEXT("origin")?TEXT("Origin (Development)"):Language==TEXT("en-US")?TEXT("English"):Language==TEXT("zh-Hans")?TEXT("简体中文"):Language);
   Combo->SetSelectedIndex(Codes.IndexOfByKey(Languages->GetLanguage()));
   Combo->OnSelectionChanged.AddUniqueDynamic(this,&UVTGMainMenu::LanguageSelected);
  }
 }
 if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("Button"))))
  Button->OnClicked.AddUniqueDynamic(this,&UVTGMainMenu::ContinueGame);
 RefreshContinue();
}
void UVTGMainMenu::LanguageSelected(FString,ESelectInfo::Type)
{
 if(auto* Languages=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGLocalizationSubsystem>():nullptr)
  if(auto* Combo=Cast<UComboBoxString>(GetWidgetFromName(TEXT("TextLanguageSelector"))))
  {
   const auto Codes=Languages->GetLanguages();const int32 Index=Combo->GetSelectedIndex();
   if(Codes.IsValidIndex(Index))Languages->SetLanguage(Codes[Index]);
  }
}
void UVTGMainMenu::RefreshContinue()
{
 auto* Save=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 const int32 LatestSlot=Save?Save->GetLatestValidSlot():INDEX_NONE;
 const bool Available=LatestSlot!=INDEX_NONE;
 UE_LOG(LogTemp,Display,TEXT("MainMenu Continue: latest valid slot=%d, enabled=%d"),LatestSlot,Available);
 if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("Button"))))
 {
  Button->SetIsEnabled(Available);
  Button->SetToolTipText(Available?NSLOCTEXT("VTGMainMenu","PickSlot","Choose a save to continue"):NSLOCTEXT("VTGMainMenu","NoSave","No save available"));
 }
}
void UVTGMainMenu::ContinueGame()
{
 auto* Save=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 if(!Save||Save->GetLatestValidSlot()==INDEX_NONE){RefreshContinue();return;}

 if(!SavePage)SavePage=CreateWidget<UVTGTerminalSavePage>(GetOwningPlayer(),UVTGTerminalSavePage::StaticClass(),TEXT("MainMenuSavePage"));
 if(!SavePage)
 {
  // Page could not be built: fall back to the old behaviour rather than dead-ending the button.
  if(!Save->ContinueLatestSave()){RefreshContinue();return;}
  SetIsEnabled(false);
  UGameplayStatics::SetGamePaused(this,false);
  if(auto* PC=GetOwningPlayer()){PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
  return;
 }

 if(!SavePage->IsInViewport())SavePage->AddToViewport(50);
 SavePage->OpenForMainMenu(this);
 // The page is its own viewport widget; hand it the screen and let its CLOSE give it back.
 SetVisibility(ESlateVisibility::Collapsed);
 UGameplayStatics::SetGamePaused(this,false);
 if(auto* PC=GetOwningPlayer()){PC->SetInputMode(FInputModeUIOnly());PC->bShowMouseCursor=true;}
}
