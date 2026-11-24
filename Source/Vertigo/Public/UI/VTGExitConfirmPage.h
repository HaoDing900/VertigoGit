#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/VTGLanguageSettingsPage.h"
#include "VTGExitConfirmPage.generated.h"

class UButton;
class UTextBlock;
class UTexture2D;
class UFont;
class UVTGLocalizationSubsystem;

/**
 * Asks where the player is actually going before the game ends. Built the same way as the language
 * page so both settings cards share one frame, font set and layout grid.
 */
UCLASS()
class VERTIGO_API UVTGExitConfirmPage : public UUserWidget
{
 GENERATED_BODY()
public:
 UVTGExitConfirmPage(const FObjectInitializer& ObjectInitializer);
 void Configure(UVTGLocalizationSubsystem* InLanguages, UTexture2D* Frame, TSoftObjectPtr<UWorld> InMainMenuLevel);
 void FocusSelection();
 UPROPERTY() FVTGSettingsBack OnBackRequested;
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
private:
 void BuildPage(UTexture2D* Frame);
 UFUNCTION() void RequestBack();
 UFUNCTION() void ReturnToMainMenu();
 UFUNCTION() void QuitToDesktop();
 UFUNCTION() void RefreshLanguage(const FString& Language);
 /** Leaving a paused game has to undo the pause and the cursor the pause menu put up. */
 void ReleasePause();
 UPROPERTY(Transient) TObjectPtr<UVTGLocalizationSubsystem> Languages;
 UPROPERTY(Transient) TObjectPtr<UButton> MainMenuButton;
 UPROPERTY(Transient) TObjectPtr<UButton> DesktopButton;
 UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Question;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> MainMenuLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> DesktopLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> BackLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> WarningLabel;
 // Native default references retain every font weight when cooking the project.
 UPROPERTY() TArray<TObjectPtr<UFont>> FontAssets;
 TSoftObjectPtr<UWorld> MainMenuLevel;
 bool bLeaving = false;
};
