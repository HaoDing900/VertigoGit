#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
namespace VTGAssetDisplayLocalizationDetails
{
void Startup();
void Shutdown();
bool IsSafeText(TSharedPtr<IPropertyHandle> Property);
bool GetMailContext(TSharedPtr<IPropertyHandle> Property,FString& Context);
}
