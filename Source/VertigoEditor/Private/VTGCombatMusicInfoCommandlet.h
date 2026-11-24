#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VTGCombatMusicInfoCommandlet.generated.h"

/**
 * Combat music and the reusable in-game info page.
 *
 *   -run=VTGCombatMusicInfo                  read-only: report every track, the page tables and the wiring
 *   -run=VTGCombatMusicInfo -Apply           build the tables, rewire the three fights, rebuild WBP_InfoInGame
 *   -run=VTGCombatMusicInfo -Apply -Reseed   also put rows back to the baseline this file holds
 *
 * Apply is re-runnable: wiring it already installed is recognised and left alone, and wording edited
 * in the editor survives a plain Apply.
 */
UCLASS()
class UVTGCombatMusicInfoCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVTGCombatMusicInfoCommandlet();
	virtual int32 Main(const FString& Params) override;
};
