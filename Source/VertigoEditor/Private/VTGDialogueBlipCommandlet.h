#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VTGDialogueBlipCommandlet.generated.h"
UCLASS()
class UVTGDialogueBlipCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
