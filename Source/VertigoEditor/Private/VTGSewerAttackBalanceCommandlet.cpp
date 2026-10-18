#include "VTGSewerAttackBalanceCommandlet.h"
#include "Engine/DataTable.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/VTGCombatPresentation.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace SewerBalance
{
FProperty* Field(const UStruct* S,const FString& Name){for(TFieldIterator<FProperty> It(S);It;++It)if(It->GetName()==Name || It->GetName().StartsWith(Name+TEXT("_")))return *It;checkNoEntry();return nullptr;}
void Backup(UObject* O)
{
 const auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const auto To=FPaths::ProjectDir()/TEXT("Scripts/SewerAttackBalance/Backup")/FPaths::GetCleanFilename(File);IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true);
 if(!IFileManager::Get().FileExists(*To))check(IFileManager::Get().Copy(*To,*File)==COPY_OK);
}
void Save(UObject* O){auto* P=O->GetOutermost();P->FullyLoad();ResetLoaders(P);P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));}
}
UVTGSewerAttackBalanceCommandlet::UVTGSewerAttackBalanceCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSewerAttackBalanceCommandlet::Main(const FString& Params)
{
 using namespace SewerBalance;
 const bool Apply=FParse::Param(*Params,TEXT("Apply")),Verify=FParse::Param(*Params,TEXT("Verify"));
 const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
 auto* Table=LoadObject<UDataTable>(nullptr,*(Root+TEXT("DT_SeaMonster_Phases")));check(Table);
 auto* Egg=LoadObject<UAnimMontage>(nullptr,*(Root+TEXT("Montages/AM_SeaMonsterEgg_ReleaseGas")));check(Egg);
 const auto Out=FPaths::ProjectDir()/TEXT("Scripts/SewerAttackBalance");
 FFileHelper::SaveStringToFile(Table->GetTableAsJSON(),*(Out/(Apply?TEXT("abilities-before.json"):Verify?TEXT("abilities-verified.json"):TEXT("abilities-audit.json"))));
 if(Apply){Backup(Table);Backup(Egg);}
 auto* Array=CastFieldChecked<FArrayProperty>(Field(Table->GetRowStruct(),TEXT("Abilities")));UScriptStruct* AbilityType=CastFieldChecked<FStructProperty>(Array->Inner)->Struct;
 auto* Name=Field(AbilityType,TEXT("AbilityName"));auto* Modifier=CastFieldChecked<FStructProperty>(Field(AbilityType,TEXT("Modifier")));
 auto* Row=Table->FindRowUnchecked(TEXT("SeaMonster_P1"));check(Row);FScriptArrayHelper Abilities(Array,Array->ContainerPtrToValuePtr<void>(Row));
 int32 GasCount=0;const double NewRadius=700.*FMath::Sqrt(.85);
 for(int32 I=0;I<Abilities.Num();++I)
 {
  auto* Data=Abilities.GetRawPtr(I);FString Label;
  if(auto* P=CastField<FNameProperty>(Name))Label=P->GetPropertyValue_InContainer(Data).ToString();else Label=CastFieldChecked<FStrProperty>(Name)->GetPropertyValue_InContainer(Data);
  UE_LOG(LogTemp,Display,TEXT("BOSS ABILITY %d: %s"),I,*Label);
  if(Label!=TEXT("Toxic Gas"))continue;
  ++GasCount;auto* Vector=Modifier->ContainerPtrToValuePtr<FVector>(Data);
  if(Apply){check(FMath::IsNearlyEqual(Vector->X,700.,.001) || FMath::IsNearlyEqual(Vector->X,NewRadius,.001));Vector->X=NewRadius;}
  if(Verify)check(FMath::IsNearlyEqual(Vector->X,NewRadius,.001));
  UE_LOG(LogTemp,Display,TEXT("BOSS GAS radius=%.6f area ratio to old=%.6f"),Vector->X,FMath::Square(Vector->X/700.));
 }
 check(GasCount==1);
 int32 Windows=0;for(auto& N:Egg->Notifies)
 {
  UE_LOG(LogTemp,Display,TEXT("EGG NOTIFY %s time=%.6f duration=%.6f"),*N.NotifyName.ToString(),N.GetTime(),N.GetDuration());
  if(!N.NotifyStateClass || N.NotifyStateClass->GetClass()->GetName()!=TEXT("Notify_Damage_C"))continue;
  ++Windows;const float Duration=N.GetDuration();
  if(Apply){check(FMath::IsNearlyEqual(N.GetTime(),.85f,.0001f) || FMath::IsNearlyEqual(N.GetTime(),1.35f,.0001f));check(1.35f+Duration<Egg->GetPlayLength());N.Link(Egg,1.35f);N.SetDuration(Duration);N.EndLink.Link(Egg,1.35f+Duration);}
  if(Verify)check(FMath::IsNearlyEqual(N.GetTime(),1.35f,.0001f) && FMath::IsNearlyEqual(Duration,.3f,.0001f));
  UE_LOG(LogTemp,Display,TEXT("EGG DAMAGE %.6f to %.6f montage rate=%.3f"),N.GetTime(),N.GetTime()+N.GetDuration(),Egg->RateScale);
 }
 check(Windows==1);
 // Boss VFX must still obtain its dimensions from the live ability radius.
 auto* BossMontage=LoadObject<UAnimMontage>(nullptr,*(Root+TEXT("Montages/AM_EggProducer_ToxicGas")));check(BossMontage);bool Synced=false;
 for(const auto& N:BossMontage->Notifies)if(auto* FX=Cast<UVTGAttackEffectNotify>(N.Notify)){check(FX->EffectReferenceRadius>0);Synced=true;}check(Synced);
 if(Apply){Table->PostEditChange();Egg->SortNotifies();Egg->PostEditChange();Save(Table);Save(Egg);FFileHelper::SaveStringToFile(Table->GetTableAsJSON(),*(Out/TEXT("abilities-after.json")));}
 UE_LOG(LogTemp,Display,TEXT("SEWER ATTACK BALANCE %s PASS"),Apply?TEXT("APPLY"):Verify?TEXT("VERIFY"):TEXT("AUDIT"));return 0;
}
