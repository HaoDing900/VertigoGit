#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGPauseMenu.generated.h"
class UButton;
class UTextBlock;
class UVTGLocalizationSubsystem;
class UVTGLanguageSettingsPage;
class UVTGExitConfirmPage;
class UTexture2D;
UCLASS()
class VERTIGO_API UVTGPauseMenu : public UUserWidget
{
 GENERATED_BODY()
public:
 bool HandleSettingsBack();
 UPROPERTY(EditDefaultsOnly, Category="Pause Settings") TObjectPtr<UTexture2D> SettingsFrame;
 /** Where "Return to Main Menu" goes. Authored on WBP_PauseMenu so the map can move without a rebuild. */
 UPROPERTY(EditDefaultsOnly, Category="Pause Settings") TSoftObjectPtr<UWorld> MainMenuLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/ThirdPerson/Maps/MainMenu/M_MainMenu.M_MainMenu")));
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
 UFUNCTION() void OpenLanguagePage();
 UFUNCTION() void OpenExitPage();
 UFUNCTION() void ShowMainPage();
 UFUNCTION() void RefreshLanguage(const FString& Language);
 UPROPERTY(Transient) TObjectPtr<UButton> LanguageButton;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> LanguageLabel;
 UPROPERTY(Transient) TObjectPtr<UVTGLocalizationSubsystem> Languages;
 UPROPERTY(Transient) TObjectPtr<UVTGLanguageSettingsPage> LanguagePage;
 UPROPERTY(Transient) TObjectPtr<UVTGExitConfirmPage> ExitPage;
 UPROPERTY(Transient) TObjectPtr<UButton> ExitButton;
 /** The canvas both settings cards are placed into, resolved once. */
 class UCanvasPanel* PageCanvas();
 void OpenPage(class UUserWidget* Page);
 bool bSettingsOpen = false;
};
