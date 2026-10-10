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
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"

#include "VTGAlleyRepair.inl"
#include "VTGAlleyVerify.inl"
#include "VTGMailEntryRepair.inl"
#include "VTGTerminalSaveRepair.inl"
#include "VTGLevelProgressRepair.inl"
#include "VTGMainMenuRepair.inl"
#include "VTGMapRoutes.inl"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
UVTGAlleyRepairCommandlet::UVTGAlleyRepairCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
int32 UVTGAlleyRepairCommandlet::Main(const FString& Params)
{
 if(FParse::Param(*Params,TEXT("VerifyCookMaps"))) return VerifyCookMapRules();
 if(FParse::Param(*Params,TEXT("MapRoutes"))) return InspectMapRoutes();
 if(FParse::Param(*Params,TEXT("InspectPistol")) || FParse::Param(*Params,TEXT("RepairPistol")))
 {
  auto* BS=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/AnimationAsset/Temp/Pistol_Starter/Animation/IPC/UE5/BS_MM_WalkRun_Pistol"));check(BS);
  UE_LOG(LogTemp,Display,TEXT("PISTOL SKELETON %s"),*GetPathNameSafe(BS->GetSkeleton()));
  for(int32 I=0; I<BS->GetNumberOfBlendSamples(); ++I)
  {
   const auto& Sample=BS->GetBlendSample(I);
   UE_LOG(LogTemp,Display,TEXT("PISTOL SAMPLE %d position=%s animation=%s"),I,*Sample.SampleValue.ToString(),*GetPathNameSafe(Sample.Animation));
  }
  if(FParse::Param(*Params,TEXT("RepairPistol")))
  {
   const FString File=FPackageName::LongPackageNameToFilename(BS->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
   const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/PackagingBefore/BS_MM_WalkRun_Pistol.uasset");
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
   if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
   auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/AnimationAsset/Temp/Pistol_Starter/Animation/IPC/UE5/W1_Stand_Aim_Idle_IPC"));check(Idle && Idle->GetSkeleton()==BS->GetSkeleton());
   for(int32 I=0;I<BS->GetNumberOfBlendSamples();++I)
    if(!BS->GetBlendSample(I).Animation)
    {check(BS->GetBlendSample(I).SampleValue.IsNearlyZero());check(BS->ReplaceSampleAnimation(I,Idle));}
   BS->PostEditChange();BS->MarkPackageDirty();
   FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
   check(UPackage::SavePackage(BS->GetOutermost(),BS,*File,Args));
   UE_LOG(LogTemp,Display,TEXT("PISTOL REPAIR PASS: restored zero-speed aim idle sample; all samples preserved."));
  }
  return 0;
 }
 if(FParse::Param(*Params,TEXT("MainMenu"))) return RepairMainMenu(FParse::Param(*Params,TEXT("VerifyOnly")));
 if(FParse::Param(*Params,TEXT("LevelProgress"))) return RepairLevelProgress(FParse::Param(*Params,TEXT("VerifyOnly")));
 if(FParse::Param(*Params,TEXT("TerminalSave"))) return TerminalPatch::Run(FParse::Param(*Params,TEXT("VerifyOnly")));
 if(FParse::Param(*Params,TEXT("DirectMail"))) return RepairDirectMail(FParse::Param(*Params,TEXT("VerifyOnly")));
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
 auto* Object=LoadObject<UObject>(nullptr,*Asset);
 auto* BP=Cast<UBlueprint>(Object);
 if(Object && !BP)
     if(auto* Property=FindFProperty<FObjectPropertyBase>(Object->GetClass(),TEXT("DirectorBlueprint")))
         BP=Cast<UBlueprint>(Property->GetObjectPropertyValue_InContainer(Object));
 if(!BP) return 1;
 FString Out;
 auto DumpProps=[&Out](UObject* O) {
 if(!O)return;
 Out+=TEXT("\nOBJECT ")+O->GetPathName()+TEXT(" CLASS ")+O->GetClass()->GetPathName()+TEXT("\n");
 for(TFieldIterator<FProperty> P(O->GetClass());P;++P) {
 if(P->HasAnyPropertyFlags(CPF_Transient))continue;
 FString V;P->ExportText_InContainer(0,V,O,O,O,PPF_None);
 if(V.Len()<20000)Out+=P->GetName()+TEXT("=")+V+TEXT("\n");
 }};
 DumpProps(BP->GeneratedClass->GetDefaultObject());
 if(auto* WBP=Cast<UWidgetBlueprint>(BP)) WBP->WidgetTree->ForEachWidget([&](UWidget* W){DumpProps(W);DumpProps(W->Slot);});
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
 FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectSavedDir()/TEXT("QA")/(FPaths::GetBaseFilename(Asset)+TEXT(".graph.txt"))));
 UE_LOG(LogTemp,Display,TEXT("DUMPED %s"),*Asset);
 }
 return 0;
}
