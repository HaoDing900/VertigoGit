#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGWidgetStatics.generated.h"

class UWidget;

/** Small pure helpers for UMG property bindings, which the binding system only accepts pure. */
UCLASS()
class VERTIGO_API UVTGWidgetStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * One widget's visibility expressed as "whatever these ones are doing".
	 *
	 * Bind it to a decoration that only makes sense while some other widget is on screen, instead of
	 * adding a Set Visibility next to every place that shows or hides one of them - in a graph with
	 * a dozen such places, the one that gets forgotten is what leaves the decoration stranded.
	 *
	 * Null entries are ignored, so the unused pins can simply stay empty.
	 */
	UFUNCTION(BlueprintPure, Category = "VTG|UI", meta = (DisplayName = "Visibility Following Widgets"))
	static ESlateVisibility VisibilityFollowingWidgets(const UWidget* A, const UWidget* B = nullptr,
	                                                   const UWidget* C = nullptr, const UWidget* D = nullptr,
	                                                   ESlateVisibility WhenAnyVisible = ESlateVisibility::Visible,
	                                                   ESlateVisibility WhenNoneVisible = ESlateVisibility::Collapsed);
};
