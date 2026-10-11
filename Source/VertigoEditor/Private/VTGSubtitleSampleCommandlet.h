#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSubtitleSampleCommandlet.generated.h"
UCLASS()
class UVTGSubtitleSampleCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSubtitleSampleCommandlet();
    virtual int32 Main(const FString& Params) override;
};
