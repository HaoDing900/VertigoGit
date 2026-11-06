#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGAudioMaintenanceCommandlet.generated.h"
UCLASS()
class UVTGAudioMaintenanceCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
