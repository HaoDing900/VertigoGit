#include "VTGSewerFollowupCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "NarrativeEvent.h"
#include "Components/SceneComponent.h"
#include "K2Node_SwitchName.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "Combat/VTGSmokeHazard.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "VTGSewerFollowupTests.inl"

namespace SewerFollowup
{
const FString Folder=FPaths::ProjectDir()/TEXT("Scripts/SewerFollowup/");
void Props(UObject* Object,FString& Out)
{
 if(!Object)return;Out+=TEXT("\nOBJECT ")+Object->GetPathName()+TEXT("\n");
 for(TFieldIterator<FProperty> It(Object->GetClass());It;++It)
 {if(It->HasAnyPropertyFlags(CPF_Transient))continue;FString Value;It->ExportText_InContainer(0,Value,Object,Object,Object,PPF_None);if(Value.Len()<30000)Out+=It->GetName()+TEXT("=")+Value+TEXT("\n");}
}
void Dump(const FString& Path)
{
 auto* BP=LoadObject<UBlueprint>(nullptr,*Path);check(BP);FString Out;Props(BP->GeneratedClass->GetDefaultObject(),Out);
 for(UClass* C=BP->GeneratedClass;C;C=C->GetSuperClass())if(auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);B && B->SimpleConstructionScript)
  for(auto* N:B->SimpleConstructionScript->GetAllNodes())Props(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)),Out);
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* G:Graphs)
 {
  Out+=TEXT("\nGRAPH ")+G->GetName()+TEXT("\n");for(UEdGraphNode* N:G->Nodes)
  {Out+=N->GetName()+TEXT(" | ")+N->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"),TEXT(" "))+TEXT(" | ")+N->NodeComment+TEXT("\n");
   for(auto* P:N->Pins){Out+=TEXT("  ")+P->PinName.ToString()+(P->Direction==EGPD_Input?TEXT(" IN "):TEXT(" OUT "))+P->DefaultValue+TEXT(" ")+GetPathNameSafe(P->DefaultObject);for(auto* L:P->LinkedTo)Out+=TEXT(" -> ")+L->GetOwningNode()->GetName()+TEXT(".")+L->PinName.ToString();Out+=TEXT("\n");}}
 }
 if(auto* C=Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass))if(auto* D=C->GetDialogueTemplate())for(auto* N:D->GetNodes()){Props(N,Out);for(auto* Event:N->Events)Props(Event,Out);}
 FFileHelper::SaveStringToFile(Out,*(Folder+BP->GetName()+TEXT(".txt")));
}
void Inspect()
{
 for(const TCHAR* Path:{TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision"),TEXT("/Game/Blueprints/BP_ISXInteractiveCollision"),TEXT("/Game/Blueprints/Actors/BP_ISXInteractiveCollision"),TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg"),TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer"),TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Still"),TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"),TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa")})Dump(Path);
 auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases"));check(Table);FFileHelper::SaveStringToFile(Table->GetTableAsJSON(),*(Folder+TEXT("abilities.json")));
 auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment"));check(Map);FString Out;
 for(AActor* A:Map->PersistentLevel->Actors)
 {if(!A)continue;const auto Name=A->GetClass()->GetName();if(Name.Contains(TEXT("Boss_SeaMonster")) || Name.Contains(TEXT("Player_Sa")) || Name.Contains(TEXT("BPLM_Sewer")) || Name.Contains(TEXT("ISXInteractive")) || A->GetActorLabel().Contains(TEXT("RoundDoor")))
  {Out+=TEXT("\nLABEL ")+A->GetActorLabel()+TEXT(" LOCATION ")+(A->GetRootComponent()?A->GetRootComponent()->GetRelativeLocation():FVector::ZeroVector).ToString()+TEXT("\n");Props(A,Out);for(auto* C:A->GetComponents())if(FindFProperty<FProperty>(C->GetClass(),TEXT("Health")))Props(C,Out);}}
 FFileHelper::SaveStringToFile(Out,*(Folder+TEXT("map.txt")));
}
void Save(UBlueprint* BP)
{
 const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=Folder+TEXT("Backup/")+BP->GetName()+TEXT(".uasset");IFileManager::Get().MakeDirectory(*(Folder+TEXT("Backup")),true);
 if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);
 auto* Package=BP->GetOutermost();Package->FullyLoad();ResetLoaders(Package);Package->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(Package,BP,*File,Args));
}
void Apply()
{
 auto* B=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"));check(B);
 bool Patched=false;
 for(UEdGraph* G:B->UbergraphPages)for(UEdGraphNode* N:G->Nodes)
  if(auto* Switch=Cast<UK2Node_SwitchName>(N);Switch && Switch->FindPin(TEXT("L3_RoundDoorSwitch")))
  {
   auto* Existing=Switch->FindPinChecked(TEXT("L3_RoundDoorSwitch"));check(Existing->LinkedTo.Num());
   auto* Alias=Switch->FindPin(TEXT("L3_OpenRoundDoor"));
   if(!Alias){Switch->PinNames.Add(TEXT("L3_OpenRoundDoor"));Switch->ReconstructNode();Alias=Switch->FindPinChecked(TEXT("L3_OpenRoundDoor"));Existing=Switch->FindPinChecked(TEXT("L3_RoundDoorSwitch"));}
   if(!Alias->LinkedTo.Num())check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Alias,Existing->LinkedTo[0]));Patched=true;break;
  }
 check(Patched);Save(B);
 auto* Spawn=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Ability_SpawnEggs"));check(Spawn);
 UEdGraph* G=Spawn->UbergraphPages[0];UK2Node_CallFunction* Nav=nullptr;UK2Node_CallFunction* Near=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(N))
 {if(Call->GetName()==TEXT("K2Node_CallFunction_6"))Nav=Call;if(Call->FunctionReference.GetMemberName()==TEXT("SpawnEggNearTarget"))Near=Call;}
 check(Nav);auto* Branch=Nav->FindPinChecked(TEXT("ReturnValue"))->LinkedTo[0]->GetOwningNode();
 auto* Exec=Branch->FindPinChecked(TEXT("execute"));
 auto* Loop=Exec->LinkedTo.Num()?Exec->LinkedTo[0]:(Near?Near->FindPinChecked(TEXT("execute"))->LinkedTo[0]:nullptr);check(Loop);
 if(!Near)
 {
  Near=NewObject<UK2Node_CallFunction>(G);G->AddNode(Near,false,false);Near->CreateNewGuid();Near->SetFromFunction(UVTGSewerAbilityHelpers::StaticClass()->FindFunctionByName(TEXT("SpawnEggNearTarget")));Near->AllocateDefaultPins();Near->NodePosX=Branch->NodePosX;Near->NodePosY=Branch->NodePosY-220;Near->NodeComment=TEXT("Spawn near player, on navigation/floor. Summoned eggs destroy on death.");
  auto* Self=NewObject<UK2Node_Self>(G);G->AddNode(Self,false,false);Self->CreateNewGuid();Self->AllocateDefaultPins();Self->NodePosX=Near->NodePosX-200;Self->NodePosY=Near->NodePosY+120;
  check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Self->FindPinChecked(TEXT("self")),Near->FindPinChecked(TEXT("Ability"))));
 }
 Exec->BreakAllPinLinks();check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Loop,Near->FindPinChecked(TEXT("execute"))));
 Save(Spawn);UE_LOG(LogTemp,Display,TEXT("APPLIED trigger alias and shared near-player egg spawn path"));
 auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Player);
 UEdGraphNode* LegacySet=nullptr;UEdGraphNode* Reroute=nullptr;UEdGraphNode* CurrentHealth=nullptr;
 for(UEdGraph* Graph:Player->UbergraphPages)for(UEdGraphNode* N:Graph->Nodes)
 {if(N->GetName()==TEXT("K2Node_VariableSet_56"))LegacySet=N;if(N->GetName()==TEXT("K2Node_Knot_103"))Reroute=N;if(N->GetName()==TEXT("K2Node_VariableGet_99"))CurrentHealth=N;}
 check(LegacySet && Reroute && CurrentHealth);auto* Output=Reroute->FindPinChecked(TEXT("OutputPin"));
 if(Output->LinkedTo.Contains(LegacySet->FindPinChecked(TEXT("execute"))))
 {
  auto* Continue=LegacySet->FindPinChecked(TEXT("then"));check(Continue->LinkedTo.Num()==1);auto* Feedback=Continue->LinkedTo[0];
  Output->BreakAllPinLinks();Continue->BreakAllPinLinks();check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Output,Feedback));
 }
 // Debug feedback must read current Health, rather than the now-unused setter output.
 auto* OldValue=LegacySet->FindPinChecked(TEXT("Output_Get"));auto Consumers=OldValue->LinkedTo;OldValue->BreakAllPinLinks();
 for(auto* Consumer:Consumers)check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(CurrentHealth->FindPinChecked(TEXT("Health")),Consumer));
 Save(Player);UE_LOG(LogTemp,Display,TEXT("APPLIED Sa: skip duplicate legacy health subtraction; retain damage feedback/death chain"));
}
}
UVTGSewerFollowupCommandlet::UVTGSewerFollowupCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSewerFollowupCommandlet::Main(const FString& Params){if(FParse::Param(*Params,TEXT("Runtime"))){FVTGSewerFollowupRegression::Run(FParse::Param(*Params,TEXT("SummonCleanupOnly")));return 0;}if(FParse::Param(*Params,TEXT("Apply")))SewerFollowup::Apply();SewerFollowup::Inspect();return 0;}
