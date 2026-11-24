#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerCombatFeelCommandlet.generated.h"

/** Sewer combat feel pass: boss hit reacts and headbutt, attack timing, hitstop, camera shake, dodge i-frames. */
UCLASS()
class UVTGSewerCombatFeelCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGSewerCombatFeelCommandlet();
    virtual int32 Main(const FString& Params) override;
};
