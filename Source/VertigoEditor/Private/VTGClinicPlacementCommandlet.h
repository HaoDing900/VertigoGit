#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGClinicPlacementCommandlet.generated.h"
UCLASS()
class UVTGClinicPlacementCommandlet : public UCommandlet
{
 GENERATED_BODY()
public: virtual int32 Main(const FString& Params) override;
};
