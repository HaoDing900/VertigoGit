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
#include "Private/VTGCameraRatio.h"
#include "Private/VTGCutsceneAudioDetails.h"
#include "Private/VTGBowerbirdTrace.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Editor.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"

IMPLEMENT_MODULE(FVertigoEditorModule, VertigoEditor)

static const FName NAME_LevelManagerClass("VTGLevelManagerBase");
namespace VTGSubtitleEditor {void Startup();void Shutdown();}

namespace PausePIEKeys
{
FDelegateHandle Begin,End;
TSharedPtr<FUICommandInfo> Stop;
TOptional<FInputChord> Primary,Secondary;
void Restore()
{
 if(Stop){if(Primary.IsSet())Stop->SetActiveChord(Primary.GetValue(),EMultipleKeyBindingIndex::Primary);if(Secondary.IsSet())Stop->SetActiveChord(Secondary.GetValue(),EMultipleKeyBindingIndex::Secondary);}
 Stop.Reset();Primary.Reset();Secondary.Reset();
}
void Start()
{
 if(IsRunningCommandlet())return;
 Begin=FEditorDelegates::PreBeginPIE.AddLambda([](bool){
  Stop=FInputBindingManager::Get().FindCommandInContext(TEXT("PlayWorld"),FName(TEXT("StopPlaySession")));
  if(!Stop)return;
  for(auto Index:{EMultipleKeyBindingIndex::Primary,EMultipleKeyBindingIndex::Secondary})
  {
   const auto Chord=*Stop->GetActiveChord(Index);
   if(Chord.Key==EKeys::Escape && !Chord.bShift && !Chord.bCtrl && !Chord.bAlt && !Chord.bCmd)
   {if(Index==EMultipleKeyBindingIndex::Primary)Primary=Chord;else Secondary=Chord;Stop->SetActiveChord(FInputChord(EKeys::Escape,true,false,false,false),Index);}
  }
 });
 End=FEditorDelegates::EndPIE.AddLambda([](bool){Restore();});
}
void Shutdown(){FEditorDelegates::PreBeginPIE.Remove(Begin);FEditorDelegates::EndPIE.Remove(End);Restore();}
}
void FVertigoEditorModule::StartupModule()
{
 PausePIEKeys::Start();
 VTGBowerbirdTrace::Startup();
 VTGCameraRatio::Startup();
 VTGCutsceneAudioDetails::Startup();
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
 PausePIEKeys::Shutdown();
 VTGBowerbirdTrace::Shutdown();
 VTGCameraRatio::Shutdown();
 VTGCutsceneAudioDetails::Shutdown();
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

