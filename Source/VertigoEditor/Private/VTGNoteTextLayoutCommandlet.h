#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGNoteTextLayoutCommandlet.generated.h"
UCLASS()
class UVTGNoteTextLayoutCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGNoteTextLayoutCommandlet();
    virtual int32 Main(const FString& Params) override;
};
