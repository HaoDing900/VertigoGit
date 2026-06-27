#include "Save/VTGPlayerProgressComponent.h"

UVTGPlayerProgressComponent::UVTGPlayerProgressComponent()
{
	// Pure data store - no per-frame work needed.
	PrimaryComponentTick.bCanEverTick = false;
}
