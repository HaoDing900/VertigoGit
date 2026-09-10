#include "VertigoEditor.h"
#include "PropertyEditorModule.h"
#include "VTGLevelManagerDetails.h"
#include "Private/VTGMeleeMontageTrace.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_MODULE(FVertigoEditorModule, VertigoEditor)

static const FName NAME_LevelManagerClass("VTGLevelManagerBase");

void FVertigoEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(NAME_LevelManagerClass, FOnGetDetailCustomizationInstance::CreateStatic(&FVTGLevelManagerDetails::MakeInstance));
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
 if(MeleeTraceTicker.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(MeleeTraceTicker);
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(NAME_LevelManagerClass);
	}
}
