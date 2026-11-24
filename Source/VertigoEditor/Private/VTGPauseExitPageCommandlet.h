#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGPauseExitPageCommandlet.generated.h"

/** Hands the pause menu's Exit button to the C++ confirmation page, and gives its label a stable key. */
UCLASS()
class UVTGPauseExitPageCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGPauseExitPageCommandlet();
    virtual int32 Main(const FString& Params) override;
};
