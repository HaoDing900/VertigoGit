#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
namespace VTGSubtitleLocalizationDetails
{
    void Startup();
    void Shutdown();
    bool IsSafeText(TSharedPtr<IPropertyHandle> Property);
}
