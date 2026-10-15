#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
namespace VTGHintLocalizationDetails
{
void Startup();
void Shutdown();
bool IsSafeText(TSharedPtr<IPropertyHandle> Property);
}
