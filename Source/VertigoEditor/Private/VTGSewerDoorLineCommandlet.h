#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerDoorLineCommandlet.generated.h"

/** Move the round-door comment on BPLM_SewerUnderApt from the lever pull to the frame the door actually opens. */
UCLASS()
class UVTGSewerDoorLineCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSewerDoorLineCommandlet();
    virtual int32 Main(const FString& Params) override;
};
