#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGPlayerAnimationLibrary.generated.h"

class ACharacter;
class UAnimSequence;

UCLASS()
class VERTIGO_API UVTGPlayerAnimationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Uses the existing SetBaseIdle interface. Instant interrupts current montages and evaluates the new pose immediately. */
    UFUNCTION(BlueprintCallable, Category="Animation|Player", meta=(DisplayName="Set Player Base Idle"))
    static bool SetPlayerBaseIdle(ACharacter* Player, UAnimSequence* NewIdle, bool Instant=false);
};
