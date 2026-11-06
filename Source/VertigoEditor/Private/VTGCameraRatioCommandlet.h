#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGCameraRatioCommandlet.generated.h"
UCLASS()
class UVTGCameraRatioCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGCameraRatioCommandlet();
    virtual int32 Main(const FString& Params) override;
};
