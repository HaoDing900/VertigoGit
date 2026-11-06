#include "VTGCombatSewerPolishCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/DataTable.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "AnimGraphNode_ModifyBone.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Dialogue.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Combat/VTGSewerEncounterComponent.h"
#include "Combat/VTGSewerBossPresentation.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "UObject/SavePackage.h"

namespace CombatSewerPolish
{
const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
template<class T>T* Load(const FString& Path){auto* O=LoadObject<T>(nullptr,*Path);checkf(O,TEXT("Missing %s"),*Path);return O;}
template<class T>T* Component(UBlueprint* BP,FName Name,bool Write=false)
{
 for(UClass* C=BP->GeneratedClass;C;C=C->GetSuperClass())if(auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);B && B->SimpleConstructionScript)
  for(auto* N:B->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==Name)
  {if(B==BP || !Write)return CastChecked<T>(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)));auto* H=BP->GetInheritableComponentHandler(true);FComponentKey Key(N);auto* O=H->GetOverridenComponentTemplate(Key);return CastChecked<T>(O?O:H->CreateOverridenComponentTemplate(Key));}
 checkf(false,TEXT("Missing component %s"),*Name.ToString());return nullptr;
}
FProperty* Field(const UStruct* S,const TCHAR* Name)
{for(TFieldIterator<FProperty> P(S);P;++P)if(P->GetName()==Name || P->GetName().StartsWith(FString(Name)+TEXT("_")))return *P;checkf(false,TEXT("Missing field %s"),Name);return nullptr;}
double Number(const UStruct* S,void* Data,const TCHAR* Name)
{auto* P=CastFieldChecked<FNumericProperty>(Field(S,Name));auto* V=P->ContainerPtrToValuePtr<void>(Data);return P->IsInteger()?P->GetSignedIntPropertyValue(V):P->GetFloatingPointPropertyValue(V);}
void Number(const UStruct* S,void* Data,const TCHAR* Name,double Value)
{auto* P=CastFieldChecked<FNumericProperty>(Field(S,Name));auto* V=P->ContainerPtrToValuePtr<void>(Data);if(P->IsInteger())P->SetIntPropertyValue(V,int64(Value));else P->SetFloatingPointPropertyValue(V,Value);}
FString Label(const UStruct* S,void* Data,const TCHAR* Name)
{auto* P=Field(S,Name);FString Out;P->ExportTextItem_Direct(Out,P->ContainerPtrToValuePtr<void>(Data),nullptr,nullptr,PPF_None);return Out;}
void Backup(UObject* O)
{
 const auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const auto To=FPaths::ProjectDir()/TEXT("Scripts/CombatSewerPolish/Backup")/(O->GetName()+TEXT(".uasset"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true);if(IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*To))check(IFileManager::Get().Copy(*To,*File,false)==COPY_OK);
}
void Save(UObject* O)
{auto* P=O->GetOutermost();P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));}
void Compile(UBlueprint* BP)
{FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog L;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&L);check(L.NumErrors==0);UE_LOG(LogTemp,Display,TEXT("COMBAT_SEWER_COMPILE %s errors=%d warnings=%d"),*BP->GetName(),L.NumErrors,L.NumWarnings);}
TArray<UAnimMontage*> DodgeMontages(UBlueprint* Player)
{
 TArray<UAnimMontage*> Out;auto* CDO=Player->GeneratedClass->GetDefaultObject();
 for(TFieldIterator<FObjectPropertyBase> P(Player->GeneratedClass);P;++P)if(P->GetName().Contains(TEXT("Dodge")))
  if(auto* M=Cast<UAnimMontage>(P->GetObjectPropertyValue_InContainer(CDO))){Out.AddUnique(M);UE_LOG(LogTemp,Display,TEXT("DODGE_MONTAGE %s = %s length=%.3f"),*P->GetName(),*M->GetPathName(),M->GetPlayLength());}
 return Out;
}
void Inspect()
{
 auto* Player=Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));DodgeMontages(Player);
 auto* Table=Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"));FFileHelper::SaveStringToFile(Table->GetTableAsJSON(),*(FPaths::ProjectDir()/TEXT("Scripts/CombatSewerPolish/phases-before.json")));
 auto* Projectile=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));
 for(auto* N:Projectile->SimpleConstructionScript->GetAllNodes())
 {
  auto* O=N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(Projectile->GeneratedClass));
  if(auto* S=Cast<USceneComponent>(O))UE_LOG(LogTemp,Display,TEXT("PROJECTILE_COMPONENT %s scale=%s"),*N->GetVariableName().ToString(),*S->GetRelativeScale3D().ToString());
  if(auto* S=Cast<USphereComponent>(O))UE_LOG(LogTemp,Display,TEXT("PROJECTILE_SPHERE radius=%.3f"),S->GetUnscaledSphereRadius());
  if(auto* F=Cast<UVTGAcidFlightComponent>(O))UE_LOG(LogTemp,Display,TEXT("PROJECTILE_FLIGHT range=%.3f"),F->MaxTravelDistance);
 }
 auto* D=CastChecked<UDialogueBlueprintGeneratedClass>(Load<UDialogueBlueprint>(TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Freemove"))->GeneratedClass)->GetDialogueTemplate();
 UE_LOG(LogTemp,Display,TEXT("FREEMOVE_DEFAULT auto=%d always=%d duration=%d multiplier=%.3f"),D->IsAutoAdvanceEnabled(),D->bAlwaysAutoAdvance,int(D->DefaultLineDuration),D->DefaultLineReadingTimeMultiplier);
}
}
extern bool VTGVerifyCombatSewerPolish();
extern void VTGApplyCombatSewerPolish(bool Verify);
UVTGCombatSewerPolishCommandlet::UVTGCombatSewerPolishCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGCombatSewerPolishCommandlet::Main(const FString& Params)
{
 if(FParse::Param(*Params,TEXT("Inspect"))){CombatSewerPolish::Inspect();return 0;}
 if(FParse::Param(*Params,TEXT("Tests")))return VTGVerifyCombatSewerPolish()?0:1;
 VTGApplyCombatSewerPolish(FParse::Param(*Params,TEXT("Verify")));return 0;
}

// Asset edits and runtime checks are kept below/alongside this commandlet for reproducibility.
#include "VTGCombatSewerPolishAssets.inl"
