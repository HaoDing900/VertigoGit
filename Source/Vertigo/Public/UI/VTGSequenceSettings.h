#pragma once
#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "VTGSequenceSettings.generated.h"

/** Stored on the sequence asset and included in cooked builds. Missing data means no skip. */
UCLASS(BlueprintType, EditInlineNew)
class VERTIGO_API UVTGSequenceSettings : public UAssetUserData
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sequence", meta=(DisplayName="Allow Skip", ToolTip="Default off. Enable only after reviewing the sequence's events. Narrative dialogue guards and VTG.NoSkip actor tags still apply."))
    bool bAllowSkip=false;
};
