#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGClinicFixCommandlet.generated.h"
UCLASS()
class UVTGClinicFixCommandlet : public UCommandlet
{
 GENERATED_BODY()
public: virtual int32 Main(const FString& Params) override;
};
