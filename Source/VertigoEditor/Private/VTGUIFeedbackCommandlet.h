#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VTGUIFeedbackCommandlet.generated.h"
UCLASS()
class UVTGUIFeedbackCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
