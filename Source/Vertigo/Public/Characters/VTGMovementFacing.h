#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGMovementFacing.generated.h"
class ACharacter;

UCLASS()
class VERTIGO_API UVTGMovementFacing : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Convert a desired visual world heading to a capsule-relative target without changing the mesh. */
    UFUNCTION(BlueprintPure, Category="Character|Movement")
    static FRotator MovementFacingRotation(ACharacter* Character, FRotator DesiredWorldRotation, double FacingYawOffset);
};
