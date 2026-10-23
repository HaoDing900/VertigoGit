#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGNarrativeInsertCommandlet.generated.h"
UCLASS()
class UVTGNarrativeInsertCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGNarrativeInsertCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
    virtual int32 Main(const FString& Params) override;
};
