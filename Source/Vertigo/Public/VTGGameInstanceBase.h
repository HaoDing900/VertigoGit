#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "VTGGameInstanceBase.generated.h"

/**
 * Game instance base that carries the chosen stage across map transitions.
 *
 * The GameInstance is created once when the game launches and lives until the game quits - it
 * survives every Open Level. So set CurrentStage in the PREVIOUS map (before Open Level) and it
 * will still be here when the next map loads, long before any actor's BeginPlay runs.
 *
 * Stage is a plain Name (e.g. "Stage_Bar_Intro") so you can add stages without touching C++.
 *
 * Reparent BP_VTG_GameInstance to this class.
 */
UCLASS()
class VERTIGO_API UVTGGameInstanceBase : public UGameInstance
{
	GENERATED_BODY()

public:

	/** The stage the next loaded level should initialise into. Set this before calling Open Level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FName CurrentStage = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "Stage")
	void SetCurrentStage(FName NewStage) { CurrentStage = NewStage; }

	UFUNCTION(BlueprintPure, Category = "Stage")
	FName GetCurrentStage() const { return CurrentStage; }
};
