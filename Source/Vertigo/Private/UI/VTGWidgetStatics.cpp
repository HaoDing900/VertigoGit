#include "UI/VTGWidgetStatics.h"

#include "Components/Widget.h"

ESlateVisibility UVTGWidgetStatics::VisibilityFollowingWidgets(const UWidget* A, const UWidget* B, const UWidget* C,
                                                               const UWidget* D, ESlateVisibility WhenAnyVisible,
                                                               ESlateVisibility WhenNoneVisible)
{
	// Not IsVisible: that one answers false whenever the Slate widget has not been built yet, which
	// would read as "hidden" for a widget that is merely not on screen for now. GetVisibility falls
	// back to the serialised value in that case and is otherwise the same answer.
	// Visible, HitTestInvisible and SelfHitTestInvisible all mean on screen, so a box shown as
	// HitTestInvisible counts just as much as one shown as Visible.
	for (const UWidget* Widget : {A, B, C, D})
	{
		if (!Widget)
		{
			continue;
		}
		const ESlateVisibility Current = Widget->GetVisibility();
		if (Current != ESlateVisibility::Collapsed && Current != ESlateVisibility::Hidden)
		{
			return WhenAnyVisible;
		}
	}
	return WhenNoneVisible;
}
