#pragma once
#include "CoreMinimal.h"
class ULevelSequence;
class UEdGraphNode;
namespace VTGSequenceInspector
{
    struct FFinding { FString Context; FString Node; bool bUncertain=false; };
    TArray<FFinding> Inspect(ULevelSequence* Sequence);
    TArray<FFinding> InspectEndpoint(UEdGraphNode* Endpoint,const FString& Context);
    void Startup();
    void Shutdown();
}
