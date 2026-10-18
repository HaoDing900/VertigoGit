#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGNoteStateInputCommandlet.generated.h"

UCLASS()
class UVTGNoteStateInputCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGNoteStateInputCommandlet();
    virtual int32 Main(const FString& Params) override;
};
