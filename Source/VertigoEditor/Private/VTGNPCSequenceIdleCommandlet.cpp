#include "VTGNPCSequenceIdleCommandlet.h"
#include "VTGNPCSequenceIdleTargets.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

TConstArrayView<FNPCSequenceIdleTarget> VTGNPCSequenceIdleTargets()
{
 static const FNPCSequenceIdleTarget Targets[]={
 {TEXT("/Game/Characters/ABP_NPCBasic"),TEXT("/Game/Characters/NPC/Fell/BP_Fell"),TEXT("/Game/AI/AIBehaviorSystem/Demo/Characters/Mannequins/Animations/Manny/MM_Idle"),TEXT("AnimGraphNode_Slot_10")},
 {TEXT("/Game/Characters/Ulises/ABP_Ulises"),TEXT("/Game/Characters/Ulises/BP_Ulises_NPC"),TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ANM/ANM_Ulises_Sit_Default"),TEXT("AnimGraphNode_Slot_10")},
 {TEXT("/Game/Characters/BowerBird/ABP_Bowerbird_StillNPC"),TEXT("/Game/Characters/BowerBird/BP_Bowerbird_Still"),TEXT("/Game/LevelNSequence/In_Level/L2Tunnel/ANM/ANM_Bowerbird_Stand_LookAt_02"),TEXT("AnimGraphNode_Slot_10")},
 {TEXT("/Game/Characters/IPHA-P/ABP_NPC_IPHAP"),TEXT("/Game/Characters/IPHA-P/BP_Philip_IPHA_NPC"),TEXT("/Game/Characters/IPHA-P/ANM/IPHA-Philip_DefaultStand"),TEXT("AnimGraphNode_Slot_10")},
 {TEXT("/Game/Characters/NPC/Bello/ABP_Bello_NPC"),TEXT("/Game/Characters/NPC/Bello/BP_Bello_NPC"),TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_Stand_Idle_still1"),TEXT("AnimGraphNode_Slot_6")},
 {TEXT("/Game/Characters/NPC/Fell/ABP_Fell_BasicEnmSkeleton"),TEXT("/Game/Characters/NPC/Fell/BP_Fell"),TEXT("/Game/AI/AIBehaviorSystem/Demo/Characters/Mannequins/Animations/Manny/MM_Idle"),TEXT("AnimGraphNode_Slot_10")},
 {TEXT("/Game/Characters/May/ABP_NPC_May"),TEXT("/Game/Characters/May/BP_Cine_May"),TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/ANM/ANM_LS_L5_MaySitLookingUp"),TEXT("AnimGraphNode_Slot_10")}};
 if(FParse::Param(FCommandLine::Get(),TEXT("MayOnly")))return MakeArrayView(Targets+UE_ARRAY_COUNT(Targets)-1,1);
 return MakeArrayView(Targets);
}
int32 UVTGNPCSequenceIdleCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("Probe")) || Params.Contains(TEXT("Test")))return VTGTestNPCSequenceIdle(!Params.Contains(TEXT("Probe")))?0:10;
 const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 for(const auto& T:VTGNPCSequenceIdleTargets())
 {
  auto* BP=LoadObject<UAnimBlueprint>(nullptr,T.Blueprint);check(BP && BP->TargetSkeleton);
  auto* IdleProp=FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("Base Idle Anm"));check(IdleProp);
  auto* Default=Cast<UAnimSequence>(IdleProp->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
  if(!Default){check(!Verify);Default=LoadObject<UAnimSequence>(nullptr,T.Fallback);}
  check(Default && Default->AdditiveAnimType==AAT_None && BP->TargetSkeleton->IsCompatibleForEditor(Default->GetSkeleton()));
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  if(!Verify){const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/NPCSequenceIdle/Backup")/(BP->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);}
  TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UAnimGraphNode_SaveCachedPose* Cache=nullptr;UAnimGraphNode_Slot* Body=nullptr;
  for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes){if(auto* C=Cast<UAnimGraphNode_SaveCachedPose>(N);C && C->CacheName==TEXT("Cache_BasePosture"))Cache=C;if(N->GetFName()==T.CinematicNode && G->GetName()==TEXT("AnimGraph"))Body=Cast<UAnimGraphNode_Slot>(N);}
  check(Cache && Body);
  auto* Bottom=Body;TSet<UEdGraphNode*> Seen;
  while(Bottom->FindPinChecked(TEXT("Source"))->LinkedTo.Num()==1)
  {
   auto* Next=Cast<UAnimGraphNode_Slot>(Bottom->FindPinChecked(TEXT("Source"))->LinkedTo[0]->GetOwningNode());if(!Next)break;check(!Seen.Contains(Next));Seen.Add(Next);Bottom=Next;
  }
  auto* Source=Bottom->FindPinChecked(TEXT("Source"));
  if(Source->LinkedTo.IsEmpty() && !Verify)
  {
   auto* G=Bottom->GetGraph();auto* Use=NewObject<UAnimGraphNode_UseCachedPose>(G);G->AddNode(Use,false,false);Use->CreateNewGuid();Use->SaveCachedPoseNode=Cache;Use->AllocateDefaultPins();Use->NodePosX=Bottom->NodePosX-280;Use->NodePosY=Bottom->NodePosY;Use->NodeComment=TEXT("NPC idle fallback while Sequencer releases the body slot");Use->bCommentBubbleVisible=true;check(G->GetSchema()->TryCreateConnection(Use->FindPinChecked(TEXT("Pose")),Source));
  }
  check(Source->LinkedTo.Num()==1);auto* Use=CastChecked<UAnimGraphNode_UseCachedPose>(Source->LinkedTo[0]->GetOwningNode());check(Use->SaveCachedPoseNode.Get()==Cache);
  int WarmSlots=0,Guards=0;
  for(auto* G:Graphs)
  {
   const auto Nodes=G->Nodes;for(UEdGraphNode* N:Nodes)
   {
    if(auto* S=Cast<UAnimGraphNode_Slot>(N);S && !S->FindPinChecked(TEXT("Source"))->LinkedTo.IsEmpty())
    {if(!Verify)S->Node.bAlwaysUpdateSourcePose=true;check(S->Node.bAlwaysUpdateSourcePose);++WarmSlots;}
    auto* Set=Cast<UK2Node_VariableSet>(N);if(!Set || Set->VariableReference.GetMemberName()!=TEXT("Base Idle Anm"))continue;
    auto* Exec=Set->FindPinChecked(TEXT("execute"));bool Guarded=Exec->LinkedTo.Num()==1 && Exec->LinkedTo[0]->GetOwningNode()->NodeComment==TEXT("Keep previous NPC idle when input is None");
    if(!Guarded && !Verify)
    {
     auto* Schema=GetDefault<UEdGraphSchema_K2>();auto* Gate=NewObject<UK2Node_IfThenElse>(G);G->AddNode(Gate,false,false);Gate->CreateNewGuid();Gate->AllocateDefaultPins();Gate->NodePosX=Set->NodePosX-270;Gate->NodePosY=Set->NodePosY;Gate->NodeComment=TEXT("Keep previous NPC idle when input is None");Gate->bCommentBubbleVisible=true;
     auto* Valid=NewObject<UK2Node_CallFunction>(G);G->AddNode(Valid,false,false);Valid->CreateNewGuid();Valid->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("IsValid")));Valid->AllocateDefaultPins();Valid->NodePosX=Gate->NodePosX-240;Valid->NodePosY=Gate->NodePosY+120;
     auto Inputs=Exec->LinkedTo;Exec->BreakAllPinLinks();for(auto* P:Inputs)check(Schema->TryCreateConnection(P,Gate->GetExecPin()));check(Schema->TryCreateConnection(Gate->GetThenPin(),Exec));
     // An invalid idle skips only this write, preserving the caller's later initialization steps.
     for(auto* P:Set->FindPinChecked(TEXT("then"))->LinkedTo)check(Schema->TryCreateConnection(Gate->GetElsePin(),P));
     auto* Value=Set->FindPinChecked(TEXT("Base Idle Anm"));if(Value->LinkedTo.Num()==1){check(Schema->TryCreateConnection(Value->LinkedTo[0],Valid->FindPinChecked(TEXT("Object"))));}else {check(Value->LinkedTo.IsEmpty());Valid->FindPinChecked(TEXT("Object"))->DefaultObject=Value->DefaultObject;}
     check(Schema->TryCreateConnection(Valid->FindPinChecked(TEXT("ReturnValue")),Gate->GetConditionPin()));Guarded=true;
    }
    check(Guarded);++Guards;
   }
  }
  check(Guards>=2 && WarmSlots>=3);
  if(!Verify)
  {
   IdleProp->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Default);const int I=FBlueprintEditorUtils::FindNewVariableIndex(BP,TEXT("Base Idle Anm"));check(I!=INDEX_NONE);BP->NewVariables[I].DefaultValue=Default->GetPathName();
   FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors && BP->Status!=BS_Error);
   IdleProp=FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("Base Idle Anm"));check(IdleProp);IdleProp->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Default);
   FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,A));
  }
  check(IdleProp->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject())==Default);
  UE_LOG(LogTemp,Display,TEXT("NPC IDLE VERIFY PASS %s: body slot chain -> %s; %d warm slots; %d guarded idle writes; default=%s"),*BP->GetName(),*Cache->CacheName,WarmSlots,Guards,*Default->GetPathName());
 }
 return 0;
}

