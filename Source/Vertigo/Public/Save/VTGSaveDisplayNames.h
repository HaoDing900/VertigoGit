#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGSaveDisplayNames.generated.h"

/**
 * One player-facing line. The row NAME is the internal key - a level name or a stage name - and this
 * is what gets shown instead of it.
 */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGSaveDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 显示给玩家的文字 / Shown to the player. Translated through the manual localization panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Display",
		meta = (DisplayName = "显示文字 / Display Text"))
	FText Display;
};

/**
 * Where the two tables live.
 *
 * Keeping them as assets rather than code means the wording is authored and translated like any
 * other player-facing text, and a level or stage can be renamed in code without touching what the
 * player reads - the row name just has to follow it.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "VTG Save Display Names"))
class VERTIGO_API UVTGSaveDisplayNamesSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Vertigo"); }

	/** 关卡 → 地点。行名是地图短名，例如 L2_StreetOutsideBar */
	UPROPERTY(config, EditAnywhere, Category = "Save Display",
		meta = (AllowedClasses = "/Script/Engine.DataTable", DisplayName = "地点表 / Location Names"))
	TSoftObjectPtr<UDataTable> LocationNames;

	/** stage → 调查阶段。行名是 stage 名，例如 Stage_BarStreet_L2 */
	UPROPERTY(config, EditAnywhere, Category = "Save Display",
		meta = (AllowedClasses = "/Script/Engine.DataTable", DisplayName = "调查阶段表 / Phase Names"))
	TSoftObjectPtr<UDataTable> PhaseNames;
};

UCLASS()
class VERTIGO_API UVTGSaveDisplayNames : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The place name for a save's level.
	 *
	 * Falls back to the tidied asset name so a slot is never blank while the table is being filled in;
	 * that fallback is also the signal that a row is missing.
	 */
	UFUNCTION(BlueprintPure, Category = "Save|Display")
	static FText ResolveLocation(const FString& LevelName);

	/**
	 * The investigation phase for a save's stage, or empty.
	 *
	 * Empty on purpose when the stage has no row, or no stage was recorded: the caller leaves the line
	 * out rather than printing an internal name the player was never meant to see.
	 */
	UFUNCTION(BlueprintPure, Category = "Save|Display")
	static FText ResolvePhase(FName Stage);

	/** The tidied asset name, which is what a level with no row falls back to. */
	static FText FallbackLocation(const FString& LevelName);
};
