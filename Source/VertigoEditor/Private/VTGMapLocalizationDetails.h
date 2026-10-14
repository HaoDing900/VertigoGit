#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
namespace VTGMapLocalizationDetails
{
void Startup();
void Shutdown();
bool IsSafeText(TSharedPtr<IPropertyHandle> Property);
}
