#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerFollowupCommandlet.generated.h"
UCLASS()
class UVTGSewerFollowupCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerFollowupCommandlet();
 virtual int32 Main(const FString& Params) override;
};
