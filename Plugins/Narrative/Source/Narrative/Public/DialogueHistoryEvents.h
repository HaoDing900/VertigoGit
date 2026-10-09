#pragma once
#include "CoreMinimal.h"
class UNarrativeComponent;
// Notification after variable substitution and before UI callbacks can skip/replace the line.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnNarrativeLinePresented, UNarrativeComponent*, const FText&, const FText&);
NARRATIVE_API FOnNarrativeLinePresented& OnNarrativeLinePresented();
