#include "VertigoEditor.h"
#include "PropertyEditorModule.h"
#include "VTGLevelManagerDetails.h"
#include "Private/VTGManualLocalization.h"
#include "Private/VTGDialogueLocalizationDetails.h"
#include "Private/VTGSubtitleLocalizationDetails.h"
#include "Private/VTGQuestLocalizationDetails.h"
#include "Private/VTGMapLocalizationDetails.h"
#include "Private/VTGHintLocalizationDetails.h"
#include "Private/VTGAssetDisplayLocalizationDetails.h"
#include "Private/VTGSlideDetails.h"
#include "Private/VTGMailRowJump.h"
#include "Private/VTGMeleeMontageTrace.h"
#include "Private/VTGSequenceInspector.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_MODULE(FVertigoEditorModule, VertigoEditor)

static const FName NAME_LevelManagerClass("VTGLevelManagerBase");
namespace VTGSubtitleEditor {void Startup();void Shutdown();}

void FVertigoEditorModule::StartupModule()
{
 VTGMailRowJump::Startup();
 VTGManualLocalization::Startup();
 VTGDialogueLocalizationDetails::Startup();
 VTGSubtitleLocalizationDetails::Startup(); VTGQuestLocalizationDetails::Startup();
 VTGMapLocalizationDetails::Startup();
 VTGHintLocalizationDetails::Startup();
 VTGAssetDisplayLocalizationDetails::Startup();
 VTGSequenceInspector::Startup();
 VTGSubtitleEditor::Startup();
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(NAME_LevelManagerClass, FOnGetDetailCustomizationInstance::CreateStatic(&FVTGLevelManagerDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("VTGSlide", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVTGSlideDetails::MakeInstance));
 PropertyModule.NotifyCustomizationModuleChanged();
 if(FParse::Param(FCommandLine::Get(),TEXT("VTGMeleeTrace")))
 {
  MeleeTraceTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){
   if(GEngine)for(const auto& Context:GEngine->GetWorldContexts())if(auto W=Context.World())if(W->WorldType==EWorldType::PIE||W->WorldType==EWorldType::Game)
    for(TActorIterator<ACharacter> It(W);It;++It)if(It->GetClass()->GetName()==TEXT("BP_Player_Sa_C")&&!It->FindComponentByClass<UVTGMeleeMontageTrace>())
    {auto C=NewObject<UVTGMeleeMontageTrace>(*It);It->AddOwnedComponent(C);C->RegisterComponent();}
   return true;
  }),.1f);
 }

}

void FVertigoEditorModule::ShutdownModule()
{
 VTGMailRowJump::Shutdown();
 VTGManualLocalization::Shutdown();
 VTGDialogueLocalizationDetails::Shutdown();
 VTGSubtitleLocalizationDetails::Shutdown(); VTGQuestLocalizationDetails::Shutdown();
 VTGMapLocalizationDetails::Shutdown();
 VTGHintLocalizationDetails::Shutdown();
 VTGAssetDisplayLocalizationDetails::Shutdown();
 VTGSequenceInspector::Shutdown();
 VTGSubtitleEditor::Shutdown();
 if(MeleeTraceTicker.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(MeleeTraceTicker);
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(NAME_LevelManagerClass);
 PropertyModule.UnregisterCustomPropertyTypeLayout("VTGSlide");
	}
}

