#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBossHealthLiquidCommandlet.generated.h"

/**
 * Rebuilds the boss health bar fill material as a parameterised "liquid in glass" look.
 *
 *  -Apply       rebuild M_BossHealth_Caustics, (re)create MI_BossHealth_Caustics and point
 *               WBP_BossHealthBar's HealthBar fill at the instance.
 *  -VerifyOnly  reload the assets and check every parameter, the shader compile and the wiring.
 */
UCLASS()
class UVTGBossHealthLiquidCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UVTGBossHealthLiquidCommandlet();
	virtual int32 Main(const FString& Params) override;
};
