#include "Engine/AssetManager.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectHash.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LevelSequence.h"
int32 InspectMapRoutes()
{
 FString Out;
 TArray<FString> MapFiles;IFileManager::Get().FindFilesRecursive(MapFiles,*(FPaths::ProjectContentDir()/TEXT("ThirdPerson/Maps")),TEXT("*.umap"),true,false);
 TArray<FString> MapNames;for(const auto& File:MapFiles)MapNames.Add(FPaths::GetBaseFilename(File));
 auto IsMap=[&](FString Value){for(const auto& Name:MapNames)if(Value==Name || Value.EndsWith(TEXT("/")+Name) || Value.Contains(TEXT("/")+Name+TEXT(".")))return true;return false;};
 auto InspectBP=[&](UBlueprint* BP)
 {
  if(!BP)return;
  TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
  for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)for(auto* P:N->Pins)
   if(IsMap(P->DefaultValue) || (P->DefaultObject && P->DefaultObject->IsA<UWorld>()))
    Out+=FString::Printf(TEXT("BP %s | %s | %s | %s=%s %s\n"),*BP->GetPathName(),*G->GetName(),*N->GetNodeTitle(ENodeTitleType::ListView).ToString(),*P->PinName.ToString(),*P->DefaultValue,*GetPathNameSafe(P->DefaultObject));
 };
 auto InspectObject=[&](UObject* Object)
 {
  for(TFieldIterator<FProperty> P(Object->GetClass());P;++P)
  {
   if(P->GetFName()==TEXT("ActorLabel"))continue;
   if(!P->IsA<FNameProperty>() && !P->IsA<FStrProperty>() && !P->IsA<FSoftObjectProperty>() && !P->IsA<FStructProperty>())continue;
   FString Value;P->ExportText_InContainer(0,Value,Object,Object,Object,PPF_None);
   if(IsMap(Value))Out+=FString::Printf(TEXT("OBJECT %s | %s=%s\n"),*Object->GetPathName(),*P->GetName(),*Value.Left(1200));
  }
 };
 TArray<FString> Dialogues;IFileManager::Get().FindFilesRecursive(Dialogues,*(FPaths::ProjectContentDir()/TEXT("Narrative/Dialogues/Levels")),TEXT("*.uasset"),true,false);
 for(const auto& File:Dialogues)
 {
  FString Package;FPackageName::TryConvertFilenameToLongPackageName(File,Package);
  auto* BP=LoadObject<UBlueprint>(nullptr,*Package);if(!BP)continue;InspectBP(BP);
  TArray<UObject*> Children;GetObjectsWithOuter(BP->GetOutermost(),Children,true);
  for(auto* Object:Children)InspectObject(Object);
 }
 TArray<FAssetData> Sequences;
 FAssetRegistryModule::GetRegistry().GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(),Sequences);
 for(const auto& Asset:Sequences)
 {
  if(!Asset.PackageName.ToString().StartsWith(TEXT("/Game/LevelNSequence/")))continue;
  UObject* Object=Asset.GetAsset();if(!Object)continue;
  if(auto* Property=FindFProperty<FObjectPropertyBase>(Object->GetClass(),TEXT("DirectorBlueprint")))
   InspectBP(Cast<UBlueprint>(Property->GetObjectPropertyValue_InContainer(Object)));
 }
 TArray<FString> BPFiles;IFileManager::Get().FindFilesRecursive(BPFiles,*(FPaths::ProjectContentDir()/TEXT("LevelNSequence")),TEXT("BPLM*.uasset"),true,false);
 BPFiles.Add(FPaths::ProjectContentDir()/TEXT("ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu.uasset"));
 BPFiles.Add(FPaths::ProjectContentDir()/TEXT("ThirdPerson/Blueprints/BP_Player_Sa.uasset"));
 for(const auto& File:BPFiles){FString Package;FPackageName::TryConvertFilenameToLongPackageName(File,Package);InspectBP(LoadObject<UBlueprint>(nullptr,*Package));}
 for(const auto& File:MapFiles)
 {
  FString Package;FPackageName::TryConvertFilenameToLongPackageName(File,Package);
  auto* World=LoadObject<UWorld>(nullptr,*Package);if(!World || !World->PersistentLevel)continue;
  InspectBP(World->PersistentLevel->GetLevelScriptBlueprint(true));
  for(AActor* Actor:World->PersistentLevel->Actors)
  {
   if(!Actor)continue;
   InspectObject(Actor);
   if(auto* BP=Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy)) InspectBP(BP);
   for(TFieldIterator<FProperty> P(Actor->GetClass());P;++P)
   {
    if(!P->IsA<FNameProperty>() && !P->IsA<FStrProperty>() && !P->IsA<FSoftObjectProperty>())continue;
    FString Value;P->ExportText_InContainer(0,Value,Actor,Actor,Actor,PPF_None);
    if(IsMap(Value))Out+=FString::Printf(TEXT("MAP %s | Actor %s | %s=%s\n"),*Package,*Actor->GetName(),*P->GetName(),*Value);
   }
  }
  UE_LOG(LogTemp,Display,TEXT("ROUTES inspected %s"),*Package);
 }
 FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectSavedDir()/TEXT("QA/PlayableMapRoutes.txt")));
 return 0;
}

int32 VerifyCookMapRules()
{
 FAssetRegistryModule::GetRegistry().SearchAllAssets(true);
 UAssetManager::Get().UpdateManagementDatabase(true);
 TArray<FString> Files;IFileManager::Get().FindFilesRecursive(Files,*FPaths::ProjectContentDir(),TEXT("*.umap"),true,false);
 TArray<FString> Selected;GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"),TEXT("MapsToCook"),Selected,GGameIni);
 TArray<FString> ExcludedDirectories;GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"),TEXT("DirectoriesToNeverCook"),ExcludedDirectories,GGameIni);
 int32 Errors=0;
 for(const auto& File:Files)
 {
  FString Package;FPackageName::TryConvertFilenameToLongPackageName(File,Package);
  const bool Included=Selected.ContainsByPredicate([&](const FString& Entry){return Entry.Contains(TEXT("\"")+Package+TEXT("\""));});
  const FAssetData Asset=FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Package+TEXT(".")+FPackageName::GetShortName(Package)));
  if(!Asset.IsValid()) { UE_LOG(LogTemp,Display,TEXT("MAP NOT REGISTERED %s"),*Package);continue; }
  if(Asset.IsRedirector()) { UE_LOG(LogTemp,Display,TEXT("MAP ALIAS (not a separate map) %s"),*Package);continue; }
  const auto Rule=UAssetManager::Get().GetPackageCookRule(FName(*Package));
  const bool Never=Rule==EPrimaryAssetCookRule::NeverCook || ExcludedDirectories.ContainsByPredicate([&](const FString& Entry)
  { FString Path;return FParse::Value(*Entry,TEXT("Path="),Path) && (Package==Path || Package.StartsWith(Path+TEXT("/"))); });
  if(Included==Never)++Errors;
  UE_LOG(LogTemp,Display,TEXT("MAP RULE %s included=%d nevercook=%d"),*Package,Included,Never);
 }
 UE_LOG(LogTemp,Display,TEXT("MAP RULES errors=%d"),Errors);
 return Errors?1:0;
}
