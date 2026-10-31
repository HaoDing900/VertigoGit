#pragma once
#include "CoreMinimal.h"
struct FNPCSequenceIdleTarget
{
 const TCHAR* Blueprint;
 const TCHAR* Actor;
 const TCHAR* Fallback;
 const TCHAR* CinematicNode;
};
TConstArrayView<FNPCSequenceIdleTarget> VTGNPCSequenceIdleTargets();
bool VTGTestNPCSequenceIdle(bool RequireFixed);
