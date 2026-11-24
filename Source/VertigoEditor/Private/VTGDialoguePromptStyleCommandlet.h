#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGDialoguePromptStyleCommandlet.generated.h"

UCLASS()
class UVTGDialoguePromptStyleCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGDialoguePromptStyleCommandlet();
    virtual int32 Main(const FString& Params) override;
};
