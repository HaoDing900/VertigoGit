#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerFeelAuditCommandlet.generated.h"

/** Read-only dump of the numbers that decide sewer combat feel: attack timing windows, dodge windows, boss move data. */
UCLASS()
class UVTGSewerFeelAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSewerFeelAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
