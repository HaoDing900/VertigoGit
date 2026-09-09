#pragma once

#include "Commandlets/Commandlet.h"
#include "VTGRepairMeleeCommandlet.generated.h"

/** Targeted, opt-in repair for BP_Player_Sa. Does not run during normal editor startup. */
UCLASS()
class UVTGRepairMeleeCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UVTGRepairMeleeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
