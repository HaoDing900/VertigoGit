#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGNoteCallbackCommandlet.generated.h"
UCLASS()
class UVTGNoteCallbackCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGNoteCallbackCommandlet();
    virtual int32 Main(const FString& Params) override;
};
