#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerBossIntroCommandlet.generated.h"

/** Read-only check for the boss intro line fix: the reflected names it needs, and which recovery path the authored dialogues take. */
UCLASS()
class UVTGSewerBossIntroCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSewerBossIntroCommandlet();
    virtual int32 Main(const FString& Params) override;
};
