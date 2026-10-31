#include "VTGClinicPlacementCommandlet.h"
#include "VTGClinicPlacement.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

bool VTGTestClinicPlacement();
int32 UVTGClinicPlacementCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("Test")))return VTGTestClinicPlacement()?0:10;
 const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic"));check(BP);UEdGraph* G=BP->UbergraphPages[0];
 UK2Node_CallFunction* Old=nullptr;UK2Node_CallFunction* Ulises=nullptr;UK2Node_CallFunction* Placement=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
 {if(C->GetName()==TEXT("K2Node_CallFunction_76"))Old=C;if(C->GetName()==TEXT("K2Node_CallFunction_78"))Ulises=C;if(C->FunctionReference.GetMemberName()==TEXT("PlaceClinicPlayer"))Placement=C;}
 check(Ulises);
 if(!Placement && !Verify)
 {
  check(Old && Old->FunctionReference.GetMemberName()==TEXT("K2_SetActorLocationAndRotation"));
  auto* Player=Old->FindPinChecked(TEXT("self"))->LinkedTo[0];auto* Target=Ulises->FindPinChecked(TEXT("self"))->LinkedTo[0];
  FString Location=Old->FindPinChecked(TEXT("NewLocation"))->DefaultValue;
  TArray<FString> Rotation;Old->FindPinChecked(TEXT("NewRotation"))->DefaultValue.ParseIntoArray(Rotation,TEXT(","),true);check(Rotation.Num()==3);const FString FallbackYaw=Rotation[1];
  auto Inputs=Old->FindPinChecked(TEXT("execute"))->LinkedTo;auto Outputs=Old->FindPinChecked(TEXT("then"))->LinkedTo;Old->BreakAllNodeLinks();
  auto* Schema=GetDefault<UEdGraphSchema_K2>();for(auto* A:Inputs)for(auto* B:Outputs)check(Schema->TryCreateConnection(A,B));
  FBlueprintEditorUtils::RemoveNode(BP,Old,true);
  Placement=NewObject<UK2Node_CallFunction>(G);G->AddNode(Placement,false,false);Placement->CreateNewGuid();Placement->SetFromFunction(UVTGClinicPlacementLibrary::StaticClass()->FindFunctionByName(TEXT("PlaceClinicPlayer")));Placement->AllocateDefaultPins();Placement->NodePosX=Ulises->NodePosX+300;Placement->NodePosY=Ulises->NodePosY;Placement->NodeComment=TEXT("Face placed Ulises; stop movement and hold input until Narrative takes ownership");Placement->bCommentBubbleVisible=true;
  auto Next=Ulises->FindPinChecked(TEXT("then"))->LinkedTo;Ulises->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
  check(Schema->TryCreateConnection(Ulises->FindPinChecked(TEXT("then")),Placement->FindPinChecked(TEXT("execute"))));for(auto* L:Next)check(Schema->TryCreateConnection(Placement->FindPinChecked(TEXT("then")),L));
  check(Schema->TryCreateConnection(Player,Placement->FindPinChecked(TEXT("Player"))));check(Schema->TryCreateConnection(Target,Placement->FindPinChecked(TEXT("FacingTarget"))));Schema->TrySetDefaultValue(*Placement->FindPinChecked(TEXT("Location")),Location);
  Schema->TrySetDefaultValue(*Placement->FindPinChecked(TEXT("FallbackYaw")),FallbackYaw);
 }
 float RequestedFallback=0.f;if(!Verify && FParse::Value(*Params,TEXT("FallbackYaw="),RequestedFallback))GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Placement->FindPinChecked(TEXT("FallbackYaw")),FString::SanitizeFloat(RequestedFallback));
 check(Placement && Ulises->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Placement->FindPinChecked(TEXT("execute"))));
 check(Placement->FindPinChecked(TEXT("Player"))->LinkedTo.Num()==1 && Placement->FindPinChecked(TEXT("FacingTarget"))->LinkedTo.Num()==1);
 if(!Verify)
 {
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors && BP->Status!=BS_Error);
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/ClinicFacingDiagnosis/Backup/BPLM_Clinic.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,A));
 }
 UE_LOG(LogTemp,Display,TEXT("CLINIC PLACEMENT VERIFY PASS: NPC placed first, then stopped player faces NPC with transition input lease"));return 0;
}
