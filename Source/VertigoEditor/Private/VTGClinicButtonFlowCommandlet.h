#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGClinicButtonFlowCommandlet.generated.h"
UCLASS()
class UVTGClinicButtonFlowCommandlet : public UCommandlet
{
 GENERATED_BODY()
public: virtual int32 Main(const FString& Params) override;
};
