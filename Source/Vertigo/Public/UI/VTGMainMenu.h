#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGMainMenu.generated.h"
UCLASS()
class VERTIGO_API UVTGMainMenu : public UUserWidget
{
 GENERATED_BODY()
protected:
 virtual void NativeConstruct() override;
private:
 UFUNCTION() void ContinueGame();
 void RefreshContinue();
};
