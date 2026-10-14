#pragma once
#include "CoreMinimal.h"
class IDetailLayoutBuilder;

/** Allows project editor tools to extend node details without replacing Narrative's customization. */
DECLARE_MULTICAST_DELEGATE_OneParam(FNarrativeDialogueDetailsExtension, IDetailLayoutBuilder&);
NARRATIVEDIALOGUEEDITOR_API FNarrativeDialogueDetailsExtension& GetNarrativeDialogueDetailsExtension();
