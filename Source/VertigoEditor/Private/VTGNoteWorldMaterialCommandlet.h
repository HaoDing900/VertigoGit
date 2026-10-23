#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGNoteWorldMaterialCommandlet.generated.h"

UCLASS()
class UVTGNoteWorldMaterialCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGNoteWorldMaterialCommandlet();
    virtual int32 Main(const FString& Params) override;
};
