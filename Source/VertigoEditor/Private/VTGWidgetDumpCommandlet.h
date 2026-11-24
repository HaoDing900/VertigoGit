#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGWidgetDumpCommandlet.generated.h"

/** Read-only: a Widget Blueprint's tree and the graph chains that drive it. */
UCLASS()
class UVTGWidgetDumpCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGWidgetDumpCommandlet();
    virtual int32 Main(const FString& Params) override;
};
