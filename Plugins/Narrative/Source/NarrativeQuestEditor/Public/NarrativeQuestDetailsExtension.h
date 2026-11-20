#pragma once
#include "CoreMinimal.h"
class IDetailLayoutBuilder;
DECLARE_MULTICAST_DELEGATE_OneParam(FNarrativeQuestDetailsExtension,IDetailLayoutBuilder&);
/** Project extensions run after Narrative's existing Quest node customization. */
NARRATIVEQUESTEDITOR_API FNarrativeQuestDetailsExtension& GetNarrativeQuestDetailsExtension();
/** Resolve the editor graph's live output link; runtime references can be stale in existing assets. */
NARRATIVEQUESTEDITOR_API class UQuestState* GetQuestBranchGraphDestination(class UQuestBranch* Branch);

class UQuestNode;
class SWidget;
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FNarrativeQuestLocalizationBadge, UQuestNode*);
NARRATIVEQUESTEDITOR_API FNarrativeQuestLocalizationBadge& GetNarrativeQuestLocalizationBadge();
