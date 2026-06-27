#include "Save/VTGSaveStatics.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

void UVTGSaveStatics::SerializeSaveGameProperties(UObject* Target, TArray<uint8>& OutData)
{
	if (!Target)
	{
		return;
	}

	OutData.Reset();
	FMemoryWriter MemWriter(OutData, /*bIsPersistent=*/true);
	FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bLoadIfFindFails=*/true);
	Ar.ArIsSaveGame = true;   // only properties marked "SaveGame" are touched
	Ar.ArNoDelta = true;      // write defaults too, so a load is fully deterministic
	Target->Serialize(Ar);
}

void UVTGSaveStatics::DeserializeSaveGameProperties(UObject* Target, const TArray<uint8>& InData)
{
	if (!Target || InData.Num() == 0)
	{
		return;
	}

	FMemoryReader MemReader(InData, /*bIsPersistent=*/true);
	FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bLoadIfFindFails=*/true);
	Ar.ArIsSaveGame = true;
	Target->Serialize(Ar);
}
