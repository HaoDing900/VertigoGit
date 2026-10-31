#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSaCinePoseCommandlet.generated.h"
UCLASS()
class UVTGSaCinePoseCommandlet : public UCommandlet
{
 GENERATED_BODY()
public: virtual int32 Main(const FString& Params) override;
};
