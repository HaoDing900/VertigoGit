#pragma once
#include "Blueprint/UserWidget.h"
#include "VTGResponseLockedBackdrop.generated.h"
class UImage;
class UTexture2D;
class UWidgetAnimation;

/** Temporary, non-interactive feedback below the dialogue viewport layers. */
UCLASS()
class VERTIGO_API UVTGResponseLockedBackdrop : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowFor(UUserWidget* Button,UTexture2D* Texture,float FadeStart,float FadeEnd);
    void HideFeedback();
protected:
    virtual void NativeOnInitialized() override;
private:
    friend bool VTGRunResponseTransitionChecks();
    UPROPERTY(Transient) TObjectPtr<UImage> Image;
    UPROPERTY(Transient) TObjectPtr<UWidgetAnimation> Fade;
};
