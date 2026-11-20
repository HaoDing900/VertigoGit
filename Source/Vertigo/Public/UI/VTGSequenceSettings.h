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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sequence", meta=(DisplayName="Hide Quest UI During Playback", ToolTip="Default on. Hide only Narrative3Overlay's quest area while playing or paused; its own visibility logic continues independently."))
    bool bHideQuestUI=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sequence", meta=(DisplayName="End Narrative Dialogue On Start", ToolTip="Default off. End the current dialogue when an external playback starts, ignoring Allow Exit. Instance overrides and Narrative-owned shot protection still apply."))
    bool bEndNarrativeDialogueOnStart=false;
};
