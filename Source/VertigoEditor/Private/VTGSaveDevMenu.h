#pragma once

#include "CoreMinimal.h"

/**
 * A "Vertigo" pulldown in the level editor's main menu bar, holding the save-system switches that are
 * otherwise console-only. Right now that is one: whether a run launched from the editor writes story
 * autosaves (VTG.AutoSaveInDevRuns).
 *
 * The menu entry is a thin wrapper over the two functions below and holds no state of its own - the
 * console variable remains the single source of truth, so the checkbox and a typed console command can
 * never disagree. Those two are exposed for the regression, which tests them rather than the widget.
 */
namespace VTGSaveDevMenu
{
	void Startup();
	void Shutdown();

	/** Registers the menu. Normally deferred to UToolMenus' startup callback; called directly by tests. */
	void Register();

	/** True when a run from the editor writes story autosaves. False (the default) means it does not. */
	bool IsAutoSaveEnabled();

	/** Flips the console variable. Does nothing if the Vertigo runtime module has not registered it. */
	void SetAutoSaveEnabled(bool bEnabled);

	/** False when the console variable is missing, which greys the entry out rather than lying about it. */
	bool CanToggleAutoSave();
}
