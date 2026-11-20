#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGQuestPresentationCommandlet.generated.h"
UCLASS()
class UVTGQuestPresentationCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGQuestPresentationCommandlet();
    virtual int32 Main(const FString& Params) override;
};
