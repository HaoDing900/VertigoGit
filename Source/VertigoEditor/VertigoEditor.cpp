#include "VertigoEditor.h"
#include "PropertyEditorModule.h"
#include "VTGLevelManagerDetails.h"

IMPLEMENT_MODULE(FVertigoEditorModule, VertigoEditor)

static const FName NAME_LevelManagerClass("VTGLevelManagerBase");

void FVertigoEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(NAME_LevelManagerClass, FOnGetDetailCustomizationInstance::CreateStatic(&FVTGLevelManagerDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FVertigoEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(NAME_LevelManagerClass);
	}
}
