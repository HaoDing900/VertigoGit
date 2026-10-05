#include "UI/VTGDeathScreen.h"

#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Save/VTGSaveCoordinator.h"

void UVTGDeathScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (RetryButton)
	{
		RetryButton->OnClicked.AddDynamic(this, &UVTGDeathScreen::HandleRetryClicked);
	}
	if (MainMenuButton)
	{
		MainMenuButton->OnClicked.AddDynamic(this, &UVTGDeathScreen::HandleMainMenuClicked);
	}
}

void UVTGDeathScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}

FReply UVTGDeathScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R)
	{
		Retry();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVTGDeathScreen::Retry()
{
	if (bLeaving)
	{
		return;
	}
	UVTGSaveCoordinator* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>() : nullptr;
	if (Save && Save->RetryFromCheckpoint())
	{
		Leave(); // out of the way so the fade to black reads
	}
}

void UVTGDeathScreen::ReturnToMainMenu()
{
	if (bLeaving || MainMenuLevel.IsNull())
	{
		return;
	}
	Leave();
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
}

void UVTGDeathScreen::Leave()
{
	bLeaving = true;
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}
