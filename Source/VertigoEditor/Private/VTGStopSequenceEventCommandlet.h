#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGStopSequenceEventCommandlet.generated.h"

UCLASS()
class UVTGStopSequenceEventCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGStopSequenceEventCommandlet();
 virtual int32 Main(const FString& Params) override;
};
