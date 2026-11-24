#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VTGPauseSettingsCommandlet.generated.h"
UCLASS()
class UVTGPauseSettingsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGPauseSettingsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
