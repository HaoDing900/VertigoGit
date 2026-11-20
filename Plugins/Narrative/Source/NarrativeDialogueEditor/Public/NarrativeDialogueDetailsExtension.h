#pragma once
#include "CoreMinimal.h"
class IDetailLayoutBuilder;

/** Allows project editor tools to extend node details without replacing Narrative's customization. */
DECLARE_MULTICAST_DELEGATE_OneParam(FNarrativeDialogueDetailsExtension, IDetailLayoutBuilder&);
NARRATIVEDIALOGUEEDITOR_API FNarrativeDialogueDetailsExtension& GetNarrativeDialogueDetailsExtension();
class UDialogueNode;
class SWidget;
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FNarrativeLocalizationBadge, UDialogueNode*);
NARRATIVEDIALOGUEEDITOR_API FNarrativeLocalizationBadge& GetNarrativeLocalizationBadge();
