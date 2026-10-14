#pragma once
#include "CoreMinimal.h"
class IDetailLayoutBuilder;
DECLARE_MULTICAST_DELEGATE_OneParam(FNarrativeQuestDetailsExtension,IDetailLayoutBuilder&);
/** Project extensions run after Narrative's existing Quest node customization. */
NARRATIVEQUESTEDITOR_API FNarrativeQuestDetailsExtension& GetNarrativeQuestDetailsExtension();
