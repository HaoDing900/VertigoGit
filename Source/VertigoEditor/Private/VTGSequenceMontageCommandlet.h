#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSequenceMontageCommandlet.generated.h"
UCLASS()
class UVTGSequenceMontageCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSequenceMontageCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
 virtual int32 Main(const FString& Params) override;
};
