#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
class SWidget;
namespace VTGDialogueLocalizationDetails
{
    void Startup();
    void Shutdown();
    bool IsSafeText(TSharedPtr<IPropertyHandle> Handle);
    TSharedRef<SWidget> MakeTextRow(TSharedPtr<IPropertyHandle> Property);
}
