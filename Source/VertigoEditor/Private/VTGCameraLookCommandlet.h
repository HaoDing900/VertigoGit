#pragma once
#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "VTGCameraLookCommandlet.generated.h"
UCLASS()
class UVTGCameraLookCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGCameraLookCommandlet();
    virtual int32 Main(const FString &Params) override;
};
