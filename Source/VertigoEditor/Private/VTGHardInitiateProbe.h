#pragma once
#include "VTGLevelManagerBase.h"
#include "VTGHardInitiateProbe.generated.h"

/** Stands in for a BPLM's own Custom Events so the base class contract can be asserted headlessly. */
UCLASS()
class AVTGHardInitiateProbe : public AVTGLevelManagerBase
{
    GENERATED_BODY()
  public:
    UFUNCTION() void ProbeInitiate()
    {
        ++Initiates;
        SawFromLoad.Add(DidEnterFromLoad());
        // What the hard initiate can see is the only thing that matters about a sub-stage: the switch
        // that reads it runs here, so a value that arrives later would be a value nobody acts on.
        SawSubStage.Add(GetSubStage());
    }
    UFUNCTION() void ProbeResume() { ++Resumes; }
    /** Named but wrong shape: the base class must refuse it rather than call it. */
    UFUNCTION() void ProbeWithParameter(int32 Ignored) { ++BadCalls; }

    int32 Initiates = 0;
    int32 Resumes = 0;
    int32 BadCalls = 0;
    TArray<bool> SawFromLoad;
    TArray<FName> SawSubStage;
};
