#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "VTGLanguageSettingsPage.generated.h"

class UButton;
class UTextBlock;
class UTexture2D;
class UFont;
class UVTGLocalizationSubsystem;

/** Exposes popup dismissal so Back closes the popup before leaving its page. */
UCLASS()
class VERTIGO_API UVTGLanguageComboBox : public UComboBoxString
{
 GENERATED_BODY()
public:
 void SetMenuOpen(bool bOpen);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVTGSettingsBack);

/** A self-contained settings page; the pause menu owns navigation and pause state. */
UCLASS()
class VERTIGO_API UVTGLanguageSettingsPage : public UUserWidget
{
 GENERATED_BODY()
public:
 UVTGLanguageSettingsPage(const FObjectInitializer& ObjectInitializer);
 void Configure(UVTGLocalizationSubsystem* InLanguages, UTexture2D* Frame);
 void FocusSelection();
 bool CloseDropdown();
 UPROPERTY() FVTGSettingsBack OnBackRequested;
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
private:
 void BuildPage(UTexture2D* Frame);
 UFUNCTION() void RequestBack();
 UFUNCTION() void SelectLanguage(FString SelectedItem, ESelectInfo::Type SelectionType);
 UFUNCTION() void RefreshLanguage(const FString& Language);
 UFUNCTION() UWidget* GenerateOption(FString Option);
 UPROPERTY(Transient) TObjectPtr<UVTGLocalizationSubsystem> Languages;
 UPROPERTY(Transient) TObjectPtr<UVTGLanguageComboBox> Selection;
 UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> FieldLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> HelpLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> BackLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusLabel;
 // Native default references retain every font weight when cooking the project.
 UPROPERTY() TArray<TObjectPtr<UFont>> FontAssets;
 TArray<FString> LanguageCodes;
 bool bSynchronizing = false;
};
