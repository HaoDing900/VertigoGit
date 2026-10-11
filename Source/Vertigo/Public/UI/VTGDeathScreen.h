#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGDeathScreen.generated.h"

class UButton;
class UImage;
class UTexture2D;

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
    /** Standard background. Set once in WBP_Death Class Defaults. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Background")
    TObjectPtr<UTexture2D> DefaultDeathImage;

    /** Off by default. An empty override falls back to the standard background. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Background", meta = (ExposeOnSpawn = true, DisplayName = "Use Other Death Image"))
    bool bUseOtherDeathImage = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Background", meta = (ExposeOnSpawn = true, EditCondition = "bUseOtherDeathImage"))
    TObjectPtr<UTexture2D> OtherDeathImage;

    /** Also supports changing an already-visible instance or returning it to the default image. */
    UFUNCTION(BlueprintCallable, Category = "Death Screen|Background")
    void SetDeathBackground(bool bInUseOtherDeathImage, UTexture2D* InOtherDeathImage = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Death Screen")
	void Retry();

	UFUNCTION(BlueprintCallable, Category = "Death Screen")
	void ReturnToMainMenu();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen")
	TSoftObjectPtr<UWorld> MainMenuLevel;

protected:
    virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> DeathBackgroundImage;

    UPROPERTY(Transient)
    FSlateBrush DesignerBackgroundBrush;
    bool bCapturedBackgroundBrush = false;
    void RefreshDeathBackground();
	UFUNCTION()
	void HandleRetryClicked() { Retry(); }

	UFUNCTION()
	void HandleMainMenuClicked() { ReturnToMainMenu(); }

	void Leave();

	bool bLeaving = false;
};
