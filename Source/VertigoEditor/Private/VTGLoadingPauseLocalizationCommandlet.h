#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VTGLoadingPauseLocalizationCommandlet.generated.h"

/** Repair only missing/invariant identities on the requested four display labels. */
UCLASS()
class UVTGLoadingPauseLocalizationCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGLoadingPauseLocalizationCommandlet();
    virtual int32 Main(const FString& Params) override;
};
