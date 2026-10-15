#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "VTGHintLayer.generated.h"

/** Shared Mail / cutscene hint layer. Each prompt has its own visibility. */
UCLASS()
class VERTIGO_API UVTGHintLayer : public UUserWidget
{
    GENERATED_BODY()
public:
    UVTGHintLayer(const FObjectInitializer& Initializer);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skip Hint|Localization") FText SkipHoldText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skip Hint|Localization") FText SkipActionText;
    void UpdateSkipHint(bool Visible,float Progress,const FSlateBrush& KeyBrush);
};
