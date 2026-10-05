#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGDeathScreen.generated.h"

class UButton;

/**
 * Death screen: "Retry" reloads the last checkpoint (VTG Save Coordinator -> Retry From Checkpoint),
 * "Main Menu" leaves. The R key also retries. Shows the mouse cursor while up.
 *
 * WBP_Death derives from this; its buttons are bound by name (RetryButton, MainMenuButton).
 */
UCLASS(Abstract)
class VERTIGO_API UVTGDeathScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Death Screen")
	void Retry();

	UFUNCTION(BlueprintCallable, Category = "Death Screen")
	void ReturnToMainMenu();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen")
	TSoftObjectPtr<UWorld> MainMenuLevel;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

private:
	UFUNCTION()
	void HandleRetryClicked() { Retry(); }

	UFUNCTION()
	void HandleMainMenuClicked() { ReturnToMainMenu(); }

	void Leave();

	bool bLeaving = false;
};
