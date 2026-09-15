#include "VTGStopSequenceEventCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UVTGStopSequenceEventCommandlet::UVTGStopSequenceEventCommandlet()
{
 IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 UVTGStopSequenceEventCommandlet::Main(const FString& Params)
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/NE_StopLevelSequence.NE_StopLevelSequence"));
 auto* EventClass=LoadClass<UObject>(nullptr,TEXT("/Script/Narrative.NarrativeEvent"));
 auto* ComponentClass=LoadClass<UObject>(nullptr,TEXT("/Script/Narrative.NarrativeComponent"));
 if(!BP || !EventClass || !ComponentClass || !BP->ParentClass->IsChildOf(EventClass)) return 1;
 auto* Function=ComponentClass->FindFunctionByName(TEXT("StopDialogueLevelSequence"));
 if(!Function || BP->UbergraphPages.Num()!=1) return 2;
 UEdGraph* Graph=BP->UbergraphPages[0];
 UK2Node_Event* Event=nullptr;
 UK2Node_CallFunction* Stop=nullptr;
 for(UEdGraphNode* Node:Graph->Nodes)
 {
  if(auto* E=Cast<UK2Node_Event>(Node); E && E->EventReference.GetMemberName()==TEXT("ExecuteEvent")) Event=E;
  if(auto* C=Cast<UK2Node_CallFunction>(Node); C && C->FunctionReference.GetMemberName()==TEXT("StopDialogueLevelSequence")) Stop=C;
 }
 const bool Verify=Params.Contains(TEXT("Verify"));
 if(!Verify)
 {
  // Refuse to replace an existing implementation.
  if(Event && Event->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num() && !Stop) return 3;
  if(!Event)
  {
   Event=NewObject<UK2Node_Event>(Graph);
   Graph->AddNode(Event,false,false); Event->CreateNewGuid();
   Event->EventReference.SetExternalMember(TEXT("ExecuteEvent"),EventClass);
   Event->bOverrideFunction=true; Event->AllocateDefaultPins();
  }
  if(!Stop)
  {
   Stop=NewObject<UK2Node_CallFunction>(Graph);
   Graph->AddNode(Stop,false,false); Stop->CreateNewGuid();
   Stop->SetFromFunction(Function); Stop->AllocateDefaultPins();
   Stop->NodePosX=Event->NodePosX+400; Stop->NodePosY=Event->NodePosY;
  }
  auto* Schema=GetDefault<UEdGraphSchema_K2>();
  if(!Schema->TryCreateConnection(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then),Stop->FindPinChecked(UEdGraphSchema_K2::PN_Execute)) ||
     !Schema->TryCreateConnection(Event->FindPinChecked(TEXT("NarrativeComponent")),Stop->FindPinChecked(UEdGraphSchema_K2::PN_Self))) return 4;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 }
 if(!Event || !Stop ||
    !Event->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Stop->FindPinChecked(UEdGraphSchema_K2::PN_Execute)) ||
    !Event->FindPinChecked(TEXT("NarrativeComponent"))->LinkedTo.Contains(Stop->FindPinChecked(UEdGraphSchema_K2::PN_Self))) return 5;
 FCompilerResultsLog Results;
 FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);
 if(Results.NumErrors || BP->Status==BS_Error) return 6;
 if(!Verify)
 {
  auto* Package=BP->GetOutermost();
  FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
  if(!UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args)) return 7;
 }
 UE_LOG(LogTemp,Display,TEXT("STOP_SEQUENCE_EVENT SUCCESS: ExecuteEvent -> StopDialogueLevelSequence; Target=NarrativeComponent; compile errors=%d warnings=%d; verify=%d"),Results.NumErrors,Results.NumWarnings,Verify);
 return 0;
}
