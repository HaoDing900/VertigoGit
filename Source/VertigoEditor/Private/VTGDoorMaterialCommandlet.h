#pragma once

#include "Commandlets/Commandlet.h"
#include "VTGDoorMaterialCommandlet.generated.h"

/** Installs and verifies the level-instance material control on the double door. */
UCLASS()
class UVTGDoorMaterialCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGDoorMaterialCommandlet();
    virtual int32 Main(const FString& Params) override;
};
