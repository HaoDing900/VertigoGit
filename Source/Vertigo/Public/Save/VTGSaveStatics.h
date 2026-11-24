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

	/**
	 * True for property types a text-export snapshot can round-trip safely: bools, numbers, enums,
	 * names, strings, text, containers of those, and structs whose every member is one of those.
	 * Object / interface / delegate properties are rejected - a saved pointer means nothing next
	 * session. This is the shared definition behind both the BPLM progress snapshot and the
	 * GameInstance snapshot; keep them agreeing by calling this rather than copying it.
	 */
	static bool IsValueProperty(const FProperty* Property);

	/** IsValueProperty, plus: declared in Blueprint (not native), and not marked Transient. */
	static bool IsBlueprintValueProperty(const FProperty* Property);

	/** Text-exports every IsBlueprintValueProperty on Object that is not in Excluded. */
	static void CaptureValueProperties(const UObject* Object, TMap<FName, FString>& OutValues, TMap<FName, FString>& OutTypes, const TSet<FName>& Excluded);

	/** Applies a CaptureValueProperties snapshot back, skipping anything whose CPP type changed. */
	static void ApplyValueProperties(UObject* Object, const TMap<FName, FString>& Values, const TMap<FName, FString>& Types, const TSet<FName>& Excluded);
};
