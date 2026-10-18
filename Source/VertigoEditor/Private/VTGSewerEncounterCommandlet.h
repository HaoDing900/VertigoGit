#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerEncounterCommandlet.generated.h"
UCLASS()
class UVTGSewerEncounterCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerEncounterCommandlet();
 virtual int32 Main(const FString& Params) override;
};
