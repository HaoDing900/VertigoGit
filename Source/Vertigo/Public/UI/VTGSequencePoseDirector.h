#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGSequencePoseDirector.generated.h"

class ACharacter;

/** Opt-in endpoint for a shot that reacquires a character after its mesh tick. */
UCLASS()
class VERTIGO_API UVTGSequencePoseDirector : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Sequence|Animation")
    static void RefreshSequenceCharacterPose(ACharacter* Character);
};
