#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGSaveStatics.generated.h"

/**
 * Low-level helpers that turn a UObject's "SaveGame"-tagged properties into bytes and back.
 * Used by IVTGSaveable's default implementation and the player progress component; also exposed to
 * Blueprint in case you ever want to serialise something by hand. Only properties you have ticked
 * "SaveGame" in their details are written - everything else is ignored.
 */
UCLASS()
class VERTIGO_API UVTGSaveStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Serialise only the SaveGame-tagged properties on Target into OutData. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	static void SerializeSaveGameProperties(UObject* Target, TArray<uint8>& OutData);

	/** Apply bytes produced by SerializeSaveGameProperties back onto Target. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	static void DeserializeSaveGameProperties(UObject* Target, const TArray<uint8>& InData);
};
