#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGDialogueHistoryCommandlet.generated.h"
UCLASS()
class UVTGDialogueHistoryCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGDialogueHistoryCommandlet();
    virtual int32 Main(const FString &Params) override;
};
