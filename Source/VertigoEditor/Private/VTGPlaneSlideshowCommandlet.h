#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGPlaneSlideshowCommandlet.generated.h"

UCLASS()
class UVTGPlaneSlideshowCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGPlaneSlideshowCommandlet();
    virtual int32 Main(const FString& Params) override;
};
