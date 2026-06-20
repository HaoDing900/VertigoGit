#include "VTGLevelManagerBase.h"
#include "VTGGameInstanceBase.h"
#include "Engine/World.h"

AVTGLevelManagerBase::AVTGLevelManagerBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

FName AVTGLevelManagerBase::GetActiveStage() const
{
	//Test override wins so designers can force a stage while editing.
	if (bOverrideStageForTesting)
	{
		return TestStage;
	}

	//Otherwise read the value the previous map stashed on the (persistent) game instance.
	if (const UWorld* World = GetWorld())
	{
		if (const UVTGGameInstanceBase* GI = World->GetGameInstance<UVTGGameInstanceBase>())
		{
			return GI->CurrentStage;
		}
	}

	return NAME_None;
}

void AVTGLevelManagerBase::BeginPlay()
{
	Super::BeginPlay();

	//Hand the resolved stage to the Blueprint child so it can run its Chain-A / Chain-B logic.
	OnStageBegin(GetActiveStage());
}
