#include "DialogueHistoryEvents.h"
FOnNarrativeLinePresented& OnNarrativeLinePresented()
{
    static FOnNarrativeLinePresented Event;
    return Event;
}
