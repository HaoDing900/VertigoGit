#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGTerminalSaveLibrary.generated.h"
class UUserWidget;

/** Adapts the existing terminal's authored tab widgets without replacing their layout. */
UCLASS()
class VERTIGO_API UVTGTerminalSaveLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 static constexpr uint8 SaveTabIndex = 254;
 UFUNCTION(BlueprintCallable, Category="Terminal|Save")
 static void PrepareTerminalTabs(UUserWidget* Menu);
 /** True opens the save page without saving; false continues the original tab selection. */
 UFUNCTION(BlueprintCallable, Category="Terminal|Save")
 static bool HandleTerminalSave(UUserWidget* Terminal, uint8 Index);
};
