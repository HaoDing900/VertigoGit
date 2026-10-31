#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGNPCSequenceIdleCommandlet.generated.h"
UCLASS()
class UVTGNPCSequenceIdleCommandlet : public UCommandlet
{
 GENERATED_BODY()
public: virtual int32 Main(const FString& Params) override;
};
