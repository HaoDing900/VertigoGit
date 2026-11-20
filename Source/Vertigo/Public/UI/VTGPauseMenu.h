#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGPauseMenu.generated.h"
class UButton;
class UTextBlock;
class UVTGLocalizationSubsystem;
UCLASS()
class VERTIGO_API UVTGPauseMenu : public UUserWidget
{
 GENERATED_BODY()
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
private:
 UFUNCTION() void ToggleLanguage();
 UFUNCTION() void RefreshLanguage(const FString& Language);
 UPROPERTY(Transient) TObjectPtr<UButton> LanguageButton;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> LanguageLabel;
 UPROPERTY(Transient) TObjectPtr<UVTGLocalizationSubsystem> Languages;
};
