#include "VTGSaveDevMenu.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreMisc.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "VTGSaveDevMenu"

namespace VTGSaveDevMenu
{
namespace
{
	FDelegateHandle StartupHandle;

	const TCHAR* const AutoSaveVariable = TEXT("VTG.AutoSaveInDevRuns");
	const FName MainMenuSection(TEXT("VTGProject"));

	// Looked up on use, never cached. The variable lives in the Vertigo runtime module as a file-static,
	// so the console registry is the only way to reach it from here - which is the right coupling: this
	// menu owns no state and cannot drift from what the console reports.
	IConsoleVariable* FindAutoSaveVariable()
	{
		return IConsoleManager::Get().FindConsoleVariable(AutoSaveVariable);
	}

	void BuildVertigoMenu(UToolMenu* Menu)
	{
		if (!Menu)
		{
			return;
		}
		// AddSection, not FindOrAddSection: this delegate runs each time the submenu is opened, on a
		// freshly built menu, and only AddSection takes the heading text.
		FToolMenuSection& Section = Menu->AddSection(TEXT("VTGSaveSystem"), LOCTEXT("SaveSection", "Save System"));
		Section.AddMenuEntry(
			TEXT("VTGAutoSaveInDevRuns"),
			LOCTEXT("AutoSaveLabel", "Autosave in editor runs"),
			LOCTEXT("AutoSaveTooltip",
				"Off by default: Play, Simulate and Standalone do not write story autosaves (slots 10/11), "
				"so tuning level sequences cannot churn your save files or leave the main menu's Continue "
				"pointing at a test.\n\n"
				"Turn it on to exercise autosaving from the editor, exactly as a packaged build does. "
				"Manual terminal saves and the slot 0 combat checkpoint always write either way.\n\n"
				"This is the console variable VTG.AutoSaveInDevRuns, and it resets to off when the editor "
				"restarts."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([] { SetAutoSaveEnabled(!IsAutoSaveEnabled()); }),
				FCanExecuteAction::CreateStatic(&CanToggleAutoSave),
				FIsActionChecked::CreateStatic(&IsAutoSaveEnabled)),
			EUserInterfaceActionType::ToggleButton);
	}
}

bool IsAutoSaveEnabled()
{
	const IConsoleVariable* Variable = FindAutoSaveVariable();
	return Variable && Variable->GetInt() != 0;
}

void SetAutoSaveEnabled(bool bEnabled)
{
	// SetByConsole, so this behaves exactly as typing the command does and an ini default does not win.
	if (IConsoleVariable* Variable = FindAutoSaveVariable())
	{
		Variable->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
	}
}

bool CanToggleAutoSave()
{
	return FindAutoSaveVariable() != nullptr;
}

void Register()
{
	UToolMenus* Menus = UToolMenus::Get();
	if (!Menus)
	{
		return;
	}
	UToolMenu* MainMenu = Menus->ExtendMenu(TEXT("LevelEditor.MainMenu"));
	if (!MainMenu)
	{
		return;
	}
	// Its own named section, not the main menu's unnamed one: Shutdown removes the section by name, and
	// removing NAME_None would take the engine's own entries with it.
	FToolMenuSection& Section = MainMenu->FindOrAddSection(MainMenuSection);
	FToolMenuEntry& Entry = Section.AddSubMenu(
		TEXT("Vertigo"),
		LOCTEXT("VertigoMenu", "Vertigo"),
		LOCTEXT("VertigoMenuTooltip", "Vertigo project switches."),
		FNewToolMenuDelegate::CreateStatic(&BuildVertigoMenu));
	// Before Help, so it sits at the end of the bar next to the project's own concerns rather than
	// between two engine menus.
	Entry.InsertPosition = FToolMenuInsert(TEXT("Help"), EToolMenuInsertType::Before);
}

void Startup()
{
	if (IsRunningCommandlet())
	{
		return;
	}
	StartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&Register));
}

void Shutdown()
{
	UToolMenus::UnRegisterStartupCallback(StartupHandle);
	StartupHandle.Reset();
	if (UObjectInitialized() && UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::Get()->RemoveSection(TEXT("LevelEditor.MainMenu"), MainMenuSection);
	}
}
}

#undef LOCTEXT_NAMESPACE
