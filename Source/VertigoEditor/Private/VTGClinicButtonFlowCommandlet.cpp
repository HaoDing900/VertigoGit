#include "VTGClinicButtonFlowCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableSet.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace ClinicButtonFlow
{
UEdGraphNode* Named(UEdGraph* G,const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetName()==Name)return N;return nullptr;}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(AP),B->FindPinChecked(BP)));}
void Bypass(UBlueprint* BP,UEdGraphNode* N)
{
 auto* In=N->FindPinChecked(TEXT("execute"));auto* Out=N->FindPinChecked(TEXT("then"));auto Inputs=In->LinkedTo;auto Outputs=Out->LinkedTo;N->BreakAllNodeLinks();
 for(auto* A:Inputs)for(auto* B:Outputs)check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));FBlueprintEditorUtils::RemoveNode(BP,N,true);
}
}
bool VTGTestClinicButtonFlow();
int32 UVTGClinicButtonFlowCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("Test")))return VTGTestClinicButtonFlow()?0:10;
 using namespace ClinicButtonFlow;const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic"));check(BP);UEdGraph* G=BP->UbergraphPages[0];
 auto* Reset=Named(G,TEXT("K2Node_VariableSet_7"));
 if(Reset){check(!Verify);check(CastChecked<UK2Node_VariableSet>(Reset)->VariableReference.GetMemberName()==TEXT("L5_HadPressedButton"));Bypass(BP,Reset);}
 // No post-press path may dereference the consumed button.
 auto* Disable=Named(G,TEXT("K2Node_CallFunction_118"));
 // Later migrations may reuse this freed node name (for example the Clinic placement call).
 if(Disable && CastChecked<UK2Node_CallFunction>(Disable)->FunctionReference.GetMemberName()!=TEXT("DisableInteract"))Disable=nullptr;
 if(Disable){check(!Verify);check(CastChecked<UK2Node_CallFunction>(Disable)->FunctionReference.GetMemberName()==TEXT("DisableInteract"));Bypass(BP,Disable);}
 auto* Start=Named(G,TEXT("K2Node_CustomEvent_28"));auto* Set=Named(G,TEXT("K2Node_VariableSet_13"));auto* Reaction=Named(G,TEXT("K2Node_CallFunction_63"));check(Start && Set && Reaction);
 UK2Node_IfThenElse* Gate=nullptr;UK2Node_CallFunction* Destroy=nullptr;UK2Node_VariableSet* Clear=nullptr;
 for(UEdGraphNode* N:G->Nodes){if(N->NodeComment==TEXT("Clinic button interaction is consumed once"))Gate=CastChecked<UK2Node_IfThenElse>(N);if(N->NodeComment==TEXT("Destroy consumed Clinic exam button"))Destroy=CastChecked<UK2Node_CallFunction>(N);if(N->NodeComment==TEXT("Clear consumed Clinic button reference"))Clear=CastChecked<UK2Node_VariableSet>(N);}
 if(!Gate && !Verify)
 {
  auto Add=[&](UEdGraphNode* N,int X,int Y){G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;};
  Gate=NewObject<UK2Node_IfThenElse>(G);Add(Gate,Start->NodePosX+240,Start->NodePosY);Gate->AllocateDefaultPins();Gate->NodeComment=TEXT("Clinic button interaction is consumed once");Gate->bCommentBubbleVisible=true;
  auto* Pressed=NewObject<UK2Node_VariableGet>(G);Add(Pressed,Gate->NodePosX-200,Gate->NodePosY+100);Pressed->VariableReference.SetSelfMember(TEXT("L5_HadPressedButton"));Pressed->AllocateDefaultPins();
  Start->FindPinChecked(TEXT("then"))->BreakAllPinLinks();Set->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();Link(Start,TEXT("then"),Gate,TEXT("execute"));Link(Pressed,TEXT("L5_HadPressedButton"),Gate,TEXT("Condition"));Link(Gate,TEXT("else"),Set,TEXT("execute"));
  Destroy=NewObject<UK2Node_CallFunction>(G);Add(Destroy,Set->NodePosX+250,Set->NodePosY);Destroy->SetFromFunction(AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor")));Destroy->AllocateDefaultPins();Destroy->NodeComment=TEXT("Destroy consumed Clinic exam button");Destroy->bCommentBubbleVisible=true;
  auto* Button=NewObject<UK2Node_VariableGet>(G);Add(Button,Destroy->NodePosX-200,Destroy->NodePosY+110);Button->VariableReference.SetSelfMember(TEXT("ISXColi_L5Button"));Button->AllocateDefaultPins();Link(Button,TEXT("ISXColi_L5Button"),Destroy,TEXT("self"));
  Clear=NewObject<UK2Node_VariableSet>(G);Add(Clear,Destroy->NodePosX+250,Destroy->NodePosY);Clear->VariableReference.SetSelfMember(TEXT("ISXColi_L5Button"));Clear->AllocateDefaultPins();Clear->NodeComment=TEXT("Clear consumed Clinic button reference");
  auto Outputs=Set->FindPinChecked(TEXT("then"))->LinkedTo;Set->FindPinChecked(TEXT("then"))->BreakAllPinLinks();for(auto* P:Outputs)check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Clear->FindPinChecked(TEXT("then")),P));Link(Set,TEXT("then"),Destroy,TEXT("execute"));Link(Destroy,TEXT("then"),Clear,TEXT("execute"));
 }
 check(Gate && Destroy && Clear);check(Gate->GetThenPin()->LinkedTo.IsEmpty());check(Gate->GetElsePin()->LinkedTo.Contains(Set->FindPinChecked(TEXT("execute"))));
 check(Set->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Destroy->FindPinChecked(TEXT("execute"))));check(Clear->FindPinChecked(TEXT("ISXColi_L5Button"))->LinkedTo.IsEmpty());
 check(Clear->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Reaction->FindPinChecked(TEXT("execute"))));
 if(!Verify)
 {
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors && BP->Status!=BS_Error);
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/ClinicButtonFlow/Backup/BPLM_Clinic.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,A));
 }
 UE_LOG(LogTemp,Display,TEXT("BUTTON FLOW VERIFY PASS: no reset on Ulises icon; one-shot button consumption destroys and clears actor; post-press dialogue avoids destroyed button"));return 0;
}

