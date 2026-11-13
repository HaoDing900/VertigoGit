#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGHeadacheVolumeCommandlet.generated.h"

/** Editor-only asset authoring. The delivered actor has no native runtime dependency. */
UCLASS()
class UVTGHeadacheVolumeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGHeadacheVolumeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
