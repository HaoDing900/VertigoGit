#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "VTGHintLayer.generated.h"

USTRUCT(BlueprintType)
struct FVTGHintDisplayText
{
    GENERATED_BODY()
    /** Stable argument used by existing Show Hint calls. Never translate this ID. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hint",meta=(VTGLocalizationLocked="Show Hint 的查找标识，不可翻译")) FName ID;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hint",meta=(MultiLine=true)) FText Text;
};

/** Shared Mail / cutscene hint layer. Each prompt has its own visibility. */
UCLASS()
class VERTIGO_API UVTGHintLayer : public UUserWidget
{
    GENERATED_BODY()
public:
    UVTGHintLayer(const FObjectInitializer& Initializer);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skip Hint|Localization") FText SkipHoldText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skip Hint|Localization") FText SkipActionText;
    /** Add an entry for each legacy HintDescription argument; edit only Text for localization. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Show Hint|Localization",meta=(TitleProperty="ID")) TArray<FVTGHintDisplayText> HintDescriptions;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Show Hint|Localization",meta=(MultiLine=true)) FText NewMailHintText;
    UFUNCTION(BlueprintPure,Category="Show Hint|Localization") FText ResolveHintDescription(FName ID) const;
    void UpdateSkipHint(bool Visible,float Progress,const FSlateBrush& KeyBrush);
};
