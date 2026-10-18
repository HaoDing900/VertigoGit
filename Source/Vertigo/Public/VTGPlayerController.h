#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VTGPlayerController.generated.h"

/** Keeps legacy Blueprint input-mode changes subordinate to active modal UI owners. */
UCLASS()
class VERTIGO_API AVTGPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void SetInputMode(const FInputModeDataBase& InData) override;
    virtual bool ShouldShowMouseCursor() const override;
};
