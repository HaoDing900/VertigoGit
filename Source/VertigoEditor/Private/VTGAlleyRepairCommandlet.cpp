#include "VTGAlleyRepairCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "UObject/UnrealType.h"

#include "VTGAlleyRepair.inl"
#include "VTGAlleyVerify.inl"
UVTGAlleyRepairCommandlet::UVTGAlleyRepairCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
int32 UVTGAlleyRepairCommandlet::Main(const FString& Params)
{
 if(FParse::Param(*Params,TEXT("Apply"))) return AlleyFix::Repair();
 if(FParse::Param(*Params,TEXT("Verify"))) return VerifyAlley();
 FString Path;
 TArray<FString> Paths;
 if(FParse::Value(*Params,TEXT("Asset="),Path)) Paths.Add(Path);
 else Paths = {
 TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"),
 TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation"),
 TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_AptAlley"),
 TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_AptAlley_Day1")};
 for(const FString& Asset : Paths)
 {
 auto* BP=LoadObject<UBlueprint>(nullptr,*Asset);
 if(!BP) return 1;
 FString Out;
 TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
 for(auto* G : Graphs) {
 Out+=TEXT("\nGRAPH ")+G->GetName()+TEXT("\n");
 for(UEdGraphNode* N:G->Nodes) {
 Out+=N->GetName()+TEXT(" | ")+N->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"),TEXT(" "))+TEXT(" | ")+N->NodeComment+TEXT("\n");
 for(auto* P:N->Pins) {
 Out+=TEXT("  ")+P->PinName.ToString()+(P->Direction==EGPD_Input?TEXT(" IN "):TEXT(" OUT "))+P->DefaultValue+TEXT(" ")+GetPathNameSafe(P->DefaultObject);
 for(auto* L:P->LinkedTo) Out+=TEXT(" -> ")+L->GetOwningNode()->GetName()+TEXT(".")+L->PinName.ToString();
 Out+=TEXT("\n");
 }}}
 if(auto* Class=Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass)) {
 if(auto* D=Class->GetDialogueTemplate()) for(auto* N:D->GetNodes()) {
 Out+=TEXT("\nDIALOGUE ")+N->GetName()+TEXT("\n");
 for(TFieldIterator<FProperty> P(N->GetClass());P;++P) {
 FString V; P->ExportText_InContainer(0,V,N,N,N,PPF_None);
 Out+=P->GetName()+TEXT("=")+V+TEXT("\n");
 }}}
 FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectSavedDir()/TEXT("QA")/(BP->GetName()+TEXT(".graph.txt"))));
 UE_LOG(LogTemp,Display,TEXT("DUMPED %s"),*Asset);
 }
 return 0;
}

