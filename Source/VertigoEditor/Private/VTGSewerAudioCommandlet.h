#pragma once
#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "VTGSewerAudioCommandlet.generated.h"
UCLASS()
class UVTGSewerAudioCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGSewerAudioCommandlet();
    virtual int32 Main(const FString &Params) override;
};
