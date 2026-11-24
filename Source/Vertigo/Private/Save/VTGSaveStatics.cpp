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

bool UVTGSaveStatics::IsValueProperty(const FProperty* Property)
{
	if (!Property)
	{
		return false;
	}
	if (Property->IsA<FBoolProperty>() || Property->IsA<FNumericProperty>() || Property->IsA<FEnumProperty>()
		|| Property->IsA<FNameProperty>() || Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>())
	{
		return true;
	}
	if (const FArrayProperty* Array = CastField<FArrayProperty>(Property)) return IsValueProperty(Array->Inner);
	if (const FSetProperty* Set = CastField<FSetProperty>(Property)) return IsValueProperty(Set->ElementProp);
	if (const FMapProperty* Map = CastField<FMapProperty>(Property)) return IsValueProperty(Map->KeyProp) && IsValueProperty(Map->ValueProp);
	if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
	{
		for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
		{
			if (!IsValueProperty(*It)) return false;
		}
		// Native opaque structs can hold runtime pointers that reflection never sees.
		return Struct->Struct->PropertyLink != nullptr;
	}
	return false;
}

bool UVTGSaveStatics::IsBlueprintValueProperty(const FProperty* Property)
{
	if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
	{
		return false;
	}
	const UClass* Owner = Property->GetOwnerClass();
	if (!Owner || !Owner->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return false;
	}
	return IsValueProperty(Property);
}

void UVTGSaveStatics::CaptureValueProperties(const UObject* Object, TMap<FName, FString>& OutValues, TMap<FName, FString>& OutTypes, const TSet<FName>& Excluded)
{
	OutValues.Reset();
	OutTypes.Reset();
	if (!Object)
	{
		return;
	}
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!IsBlueprintValueProperty(Property) || Excluded.Contains(Property->GetFName()))
		{
			continue;
		}
		FString Value;
		Property->ExportText_InContainer(0, Value, Object, Object, const_cast<UObject*>(Object), PPF_None);
		OutValues.Add(Property->GetFName(), Value);
		// The CPP type is the compatibility check on the way back in: if the variable's type changed
		// since the save was written, the stored text is meaningless and we skip it rather than guess.
		OutTypes.Add(Property->GetFName(), Property->GetCPPType());
	}
}

void UVTGSaveStatics::ApplyValueProperties(UObject* Object, const TMap<FName, FString>& Values, const TMap<FName, FString>& Types, const TSet<FName>& Excluded)
{
	if (!Object)
	{
		return;
	}
	for (const TPair<FName, FString>& Pair : Values)
	{
		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), Pair.Key);
		const FString* Type = Types.Find(Pair.Key);
		if (Property && IsBlueprintValueProperty(Property) && !Excluded.Contains(Pair.Key) && Type && *Type == Property->GetCPPType())
		{
			Property->ImportText_Direct(*Pair.Value, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None);
		}
	}
}
