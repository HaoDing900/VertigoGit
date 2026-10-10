#include "UI/VTGMainMenu.h"
#include "Save/VTGSaveCoordinator.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UVTGMainMenu::NativeConstruct()
{
 Super::NativeConstruct();
 if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("Button"))))
  Button->OnClicked.AddUniqueDynamic(this,&UVTGMainMenu::ContinueGame);
 RefreshContinue();
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
  Button->SetToolTipText(Available?NSLOCTEXT("VTGMainMenu","Latest","Load the most recent save"):NSLOCTEXT("VTGMainMenu","NoSave","No save available"));
 }
}
void UVTGMainMenu::ContinueGame()
{
 auto* Save=GetGameInstance()?GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 if (!Save || !Save->ContinueLatestSave()) { RefreshContinue();return; }
 SetIsEnabled(false);
 UGameplayStatics::SetGamePaused(this,false);
 if (auto* PC=GetOwningPlayer())
 { PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false; }
}
