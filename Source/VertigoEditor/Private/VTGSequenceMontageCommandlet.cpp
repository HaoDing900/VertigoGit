#include "VTGSequenceMontageCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
bool VTGTestSequenceMontages();
int32 UVTGSequenceMontageCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("Test"))) return VTGTestSequenceMontages()?0:10;
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/1PlayLevelSequence.1PlayLevelSequence"));
 if(!BP)return 1;
 const FName Name(TEXT("Suspend Montages During Sequence"));
 if(!FindFProperty<FBoolProperty>(BP->GeneratedClass,Name))
 {
  FEdGraphPinType Type; Type.PinCategory=UEdGraphSchema_K2::PC_Boolean;
  if(!FBlueprintEditorUtils::AddMemberVariable(BP,Name,Type,TEXT("false")))return 2;
  FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("Tooltip"),TEXT("Save and stop bound characters' Montages during the sequence, then resume their saved positions and play rates."));
  FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,Name,false);
 }
 TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
 int32 Count=0;
 for(auto* G:Graphs)
  for(UEdGraphNode* N:TArray<TObjectPtr<UEdGraphNode>>(G->Nodes))
   if(auto* Call=Cast<UK2Node_CallFunction>(N); Call && Call->FunctionReference.GetMemberName()==TEXT("PlayDialogueLevelSequence"))
   {
    Call->ReconstructNode();
    auto* Pin=Call->FindPin(TEXT("bSuspendMontagesDuringSequence")); if(!Pin)return 3;
    if(Pin->LinkedTo.IsEmpty())
    {
     auto* Get=NewObject<UK2Node_VariableGet>(G); G->AddNode(Get,false,false); Get->CreateNewGuid();
     Get->VariableReference.SetSelfMember(Name); Get->NodePosX=Call->NodePosX-340; Get->NodePosY=Call->NodePosY+500;
     Get->AllocateDefaultPins();
     if(!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Get->FindPinChecked(Name),Pin))return 4;
    }
    ++Count;
   }
 if(Count!=1)return 5;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
 if(Log.NumErrors)return 6;
 auto* Flag=FindFProperty<FBoolProperty>(BP->GeneratedClass,Name);
 if(!Flag || Flag->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()))return 7;
 FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
 const auto Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Args))return 8;
 UE_LOG(LogTemp,Display,TEXT("SEQUENCE MONTAGE PASS: checkbox default false, connected, Blueprint compiled and saved"));
 return 0;
}

