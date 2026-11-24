#include "VTGSaveDevMenu.h"
#include "ToolMenus.h"
#include "HAL/IConsoleManager.h"

/**
 * The "Vertigo > Save System > Autosave in editor runs" checkbox.
 *
 * What is worth asserting is not that a widget exists but that the checkbox cannot lie: it owns no
 * state, so reading it and setting it must both go through the console variable, and a value typed at
 * the console must show up in the tick. A checkbox that drifts from the thing it claims to control is
 * worse than no checkbox, because you would stop reading the Output Log line that says what happened.
 */
struct FVTGSaveDevMenuRegression
{
	static int32 Run()
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("VTG.AutoSaveInDevRuns"));
		check(Variable);                                  // the menu is useless without it
		check(VTGSaveDevMenu::CanToggleAutoSave());       // ...and says so rather than greying out
		const int32 Original = Variable->GetInt();
		check(Original == 0);                             // the shipped default, asserted not assumed

		// Reading follows the variable, including a change made behind the menu's back (a typed command).
		check(!VTGSaveDevMenu::IsAutoSaveEnabled());
		Variable->Set(1, ECVF_SetByConsole);
		check(VTGSaveDevMenu::IsAutoSaveEnabled());
		Variable->Set(0, ECVF_SetByConsole);
		check(!VTGSaveDevMenu::IsAutoSaveEnabled());

		// Writing reaches the variable, which is what the save coordinator actually reads.
		VTGSaveDevMenu::SetAutoSaveEnabled(true);
		check(Variable->GetInt() == 1 && VTGSaveDevMenu::IsAutoSaveEnabled());
		VTGSaveDevMenu::SetAutoSaveEnabled(false);
		check(Variable->GetInt() == 0 && !VTGSaveDevMenu::IsAutoSaveEnabled());

		// Setting the same value twice is not a toggle - the entry's click handler negates the current
		// value, so an idempotent setter is what keeps the two in step.
		VTGSaveDevMenu::SetAutoSaveEnabled(true);
		VTGSaveDevMenu::SetAutoSaveEnabled(true);
		check(VTGSaveDevMenu::IsAutoSaveEnabled());
		VTGSaveDevMenu::SetAutoSaveEnabled(false);

		// The menu itself: registering must add the Vertigo submenu to the main menu bar, under its own
		// named section so that unregistering cannot take the engine's entries with it.
		if (UToolMenus* Menus = UToolMenus::Get())
		{
			VTGSaveDevMenu::Register();
			UToolMenu* MainMenu = Menus->FindMenu(TEXT("LevelEditor.MainMenu"));
			check(MainMenu);
			const FToolMenuSection* Section = MainMenu->FindSection(TEXT("VTGProject"));
			check(Section);
			check(Section->FindEntry(TEXT("Vertigo")));
			// Registering twice (a hot reload re-running the startup callback) must not stack entries.
			VTGSaveDevMenu::Register();
			int32 Count = 0;
			for (const FToolMenuEntry& Entry : MainMenu->FindSection(TEXT("VTGProject"))->Blocks)
				if (Entry.Name == TEXT("Vertigo")) ++Count;
			check(Count == 1);
		}

		check(Variable->GetInt() == Original);
		UE_LOG(LogTemp, Display, TEXT("SAVE DEV MENU PASS: the Vertigo main-menu entry exists in its own section and does not stack on re-registration; the checkbox holds no state - it reads and writes VTG.AutoSaveInDevRuns, follows a value typed at the console, and its setter is idempotent. Default left at %d."), Original);
		return 0;
	}
};
