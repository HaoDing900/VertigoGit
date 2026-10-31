#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGLevelAudioCommandlet.generated.h"

// Asset authoring only. No gameplay class depends on this commandlet.
UCLASS()
class UVTGLevelAudioCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGLevelAudioCommandlet();
    virtual int32 Main(const FString& Params) override;
};
