#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSaveDisplayNamesCommandlet.generated.h"

/** Create and seed the two tables the save menu reads its place and phase wording from. */
UCLASS()
class UVTGSaveDisplayNamesCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGSaveDisplayNamesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
