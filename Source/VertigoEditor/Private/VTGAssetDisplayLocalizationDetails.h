#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
namespace VTGAssetDisplayLocalizationDetails
{
void Startup();
void Shutdown();
bool IsSafeText(TSharedPtr<IPropertyHandle> Property);
/** Locate the DataTable row a whitelisted struct field belongs to, for the catalogue context line. */
bool GetDataTableRowContext(TSharedPtr<IPropertyHandle> Property,FString& Context);
}
