#include "Save/VTGSaveDisplayNames.h"
#include "Misc/PackageName.h"

namespace VTGSaveDisplay
{
/** Rows are looked up by name, so a renamed level or stage shows the fallback rather than the wrong line. */
FText Lookup(const TSoftObjectPtr<UDataTable>& Table, FName Key)
{
	if (Key.IsNone())
	{
		return FText::GetEmpty();
	}
	// Synchronous: the save menu is already open and waiting, and these tables are a few rows each.
	const UDataTable* Loaded = Table.LoadSynchronous();
	if (!Loaded)
	{
		return FText::GetEmpty();
	}
	const FVTGSaveDisplayRow* Row = Loaded->FindRow<FVTGSaveDisplayRow>(Key, TEXT("VTGSaveDisplayNames"), /*bWarnIfMissing=*/false);
	return Row ? Row->Display : FText::GetEmpty();
}
}

FText UVTGSaveDisplayNames::FallbackLocation(const FString& LevelName)
{
	// What the save menu printed before these tables existed: the asset name, tidied up.
	return FText::AsCultureInvariant(FPackageName::GetShortName(LevelName).Replace(TEXT("_"), TEXT(" ")));
}

FText UVTGSaveDisplayNames::ResolveLocation(const FString& LevelName)
{
	if (LevelName.IsEmpty())
	{
		return FText::GetEmpty();
	}
	const auto* Settings = GetDefault<UVTGSaveDisplayNamesSettings>();
	const FName Key(*FPackageName::GetShortName(LevelName));
	const FText Named = Settings ? VTGSaveDisplay::Lookup(Settings->LocationNames, Key) : FText::GetEmpty();
	return Named.IsEmpty() ? FallbackLocation(LevelName) : Named;
}

FText UVTGSaveDisplayNames::ResolvePhase(FName Stage)
{
	const auto* Settings = GetDefault<UVTGSaveDisplayNamesSettings>();
	// No row and no stage both mean "say nothing": an unmapped stage name is exactly what the player
	// must not be shown, so there is deliberately no fallback here.
	return Settings ? VTGSaveDisplay::Lookup(Settings->PhaseNames, Stage) : FText::GetEmpty();
}
