#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGMainMenu.generated.h"
class UVTGTerminalSavePage;
UCLASS()
class VERTIGO_API UVTGMainMenu : public UUserWidget
{
 GENERATED_BODY()
protected:
 virtual void NativeConstruct() override;
private:
 UFUNCTION() void LanguageSelected(FString Item, ESelectInfo::Type SelectionType);
 UFUNCTION() void ContinueGame();
 void RefreshContinue();
 /**
  * Continue opens the same SAVE / LOAD page the in-game terminal uses, in its load-only mode, so
  * there is one slot list in the game instead of two that can disagree. Created on first use and
  * kept, because the page builds its own widget tree in C++ and has no asset to reference.
  */
 UPROPERTY(Transient) TObjectPtr<UVTGTerminalSavePage> SavePage;
};
