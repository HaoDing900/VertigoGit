#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
namespace VTGQuestLocalizationDetails
{
void Startup();
void Shutdown();
bool IsSafeText(TSharedPtr<IPropertyHandle> Property);
}
