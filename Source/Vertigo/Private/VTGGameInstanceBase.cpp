#include "VTGGameInstanceBase.h"
#include "Engine/World.h"

void UVTGGameInstanceBase::OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
 // Also cover plain OpenLevel/reload. Initial setup may replace a dummy world,
 // so only leaving a world that actually began play ends direct-entry testing.
 if (OldWorld && OldWorld != NewWorld && OldWorld->IsGameWorld() && OldWorld->HasBegunPlay())
  bHasExplicitStage = true;
 Super::OnWorldChanged(OldWorld, NewWorld);
}
