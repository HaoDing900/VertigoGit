#include "VTGSewerBossSyncCommandlet.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "RenderingThread.h"
#include "Misc/App.h"
#include "NiagaraSimCache.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/VTGCombatPresentation.h"
#include "UI/VTGBossHealthBarComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

namespace BossSync
{
const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
template<class T>T* Load(const FString& Path){auto* O=LoadObject<T>(nullptr,*Path);check(O);return O;}
void Backup(UObject* O){const auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const auto To=FPaths::ProjectDir()/TEXT("Scripts/SewerBossSync/Backup")/(O->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true);if(IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*To))check(IFileManager::Get().Copy(*To,*File)==COPY_OK);}
void Save(UObject* O){auto* P=O->GetOutermost();P->FullyLoad();ResetLoaders(P);P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));}
template<class T>T* Duplicate(T* Source,const FString& Name,const FString& Folder){if(auto* E=LoadObject<T>(nullptr,*(Folder+TEXT("/")+Name)))return E;return CastChecked<T>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(Name,Folder,Source));}
UMaterial* Material(bool Apply)
{
 const FString Path=Root+TEXT("FX/M_BossHealth_CausticHUD");
 if(!Apply)return Load<UMaterial>(Path);
 auto* Source=Load<UMaterial>(TEXT("/Engine/EditorMaterials/WidgetMaterial_Caustic"));
 UTexture* Texture=nullptr;for(UMaterialExpression* E:Source->GetExpressions())if(auto* S=Cast<UMaterialExpressionTextureSample>(E))Texture=S->Texture;check(Texture);
 auto* M=Duplicate(Source,TEXT("M_BossHealth_CausticHUD"),Root+TEXT("FX"));Backup(M);
 UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);M->MaterialDomain=MD_UI;M->BlendMode=BLEND_Translucent;M->bUsedWithEditorCompositing=false;
 auto* Tex=CastChecked<UMaterialExpressionTextureSampleParameter2D>(UMaterialEditingLibrary::CreateMaterialExpression(M,UMaterialExpressionTextureSampleParameter2D::StaticClass()));Tex->Texture=Texture;Tex->ParameterName=TEXT("CausticTexture");Tex->SamplerType=SAMPLERTYPE_Color;
 auto* UV=CastChecked<UMaterialExpressionTextureCoordinate>(UMaterialEditingLibrary::CreateMaterialExpression(M,UMaterialExpressionTextureCoordinate::StaticClass()));UV->UTiling=6;UV->VTiling=1;
 auto* Pan=CastChecked<UMaterialExpressionPanner>(UMaterialEditingLibrary::CreateMaterialExpression(M,UMaterialExpressionPanner::StaticClass()));Pan->SpeedX=.025;Pan->SpeedY=.01;
 auto* Opacity=CastChecked<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(M,UMaterialExpressionScalarParameter::StaticClass()));Opacity->ParameterName=TEXT("Opacity");Opacity->DefaultValue=.65;
 auto* Tint=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(M,UMaterialExpressionVectorParameter::StaticClass()));Tint->ParameterName=TEXT("CausticTint");Tint->DefaultValue=FLinearColor::White;
 auto* Color=CastChecked<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(M,UMaterialExpressionMultiply::StaticClass()));
 check(UMaterialEditingLibrary::ConnectMaterialExpressions(UV,TEXT(""),Pan,TEXT("Coordinate")));
 check(UMaterialEditingLibrary::ConnectMaterialExpressions(Pan,TEXT(""),Tex,TEXT("UVs")));
 check(UMaterialEditingLibrary::ConnectMaterialExpressions(Tex,TEXT("RGB"),Color,TEXT("A")));
 check(UMaterialEditingLibrary::ConnectMaterialExpressions(Tint,TEXT(""),Color,TEXT("B")));
 check(UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_EmissiveColor));
 check(UMaterialEditingLibrary::ConnectMaterialProperty(Opacity,TEXT(""),MP_Opacity));
 UMaterialEditingLibrary::RecompileMaterial(M);Save(M);return M;
}
UNiagaraSystem* Gas(bool Apply)
{
 if(!Apply)return Load<UNiagaraSystem>(Root+TEXT("FX/NS_Boss_ToxicGas"));
 auto* S=Duplicate(Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Large")),TEXT("NS_Boss_ToxicGas"),Root+TEXT("FX"));Backup(S);
 check(S->GetEmitterHandles().Num()>0);for(auto& H:S->GetEmitterHandles())
 {
  auto* D=H.GetEmitterData();D->bLocalSpace=true;
  // The original cloud has a one-second emitter delay in addition to its animation notify.
  for(auto* Script:{D->EmitterSpawnScriptProps.Script.Get(),D->EmitterUpdateScriptProps.Script.Get()})if(Script)
  {
   TArray<FNiagaraVariable> Vars;Script->RapidIterationParameters.GetParameters(Vars);
   for(auto& V:Vars)if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef() && V.GetName().ToString().Contains(TEXT("EmitterState.Loop Delay")))Script->RapidIterationParameters.SetParameterValue<float>(0,V);
  }
 }
 S->RequestCompile(false);S->WaitForCompilationComplete();Save(S);return S;
}
int32 Sample(UNiagaraSystem* System)
{
 TGuardValue<bool> Client(GIsClient,true);
 System->RequestCompile(false);System->WaitForCompilationComplete(true);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("GasSample"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 // UWorld deliberately skips FX systems in commandlets, even with rendering allowed.
 {TGuardValue<bool> RenderingWorld(PRIVATE_GIsRunningCommandlet,false);World->CreateFXSystem();}
 FlushRenderingCommands();
 auto* Owner=World->SpawnActor<AActor>();auto* FX=NewObject<UNiagaraComponent>(Owner);Owner->AddInstanceComponent(FX);FX->SetForceSolo(true);FX->SetAllowScalability(false);FX->SetAutoActivate(false);FX->SetAsset(System);FX->RegisterComponentWithWorld(World);
 World->InitializeActorsForPlay(FURL());World->BeginPlay();FX->Activate(true);World->Tick(LEVELTICK_All,.001f);
 UE_LOG(LogTemp,Display,TEXT("GAS SIM render=%d ready=%d dispatch=%p controller=%p active=%d"),FApp::CanEverRender(),System->IsReadyToRun(),FNiagaraGpuComputeDispatchInterface::Get(World),FX->GetSystemInstanceController().Get(),FX->IsActive());
 for(int Step=1;Step<=120;++Step)
 {
  FX->AdvanceSimulation(1,1.f/60);
  if(Step%15)continue;
  auto* Cache=NewObject<UNiagaraSimCache>();FNiagaraSimCacheCreateParameters P;if(!Cache->BeginWrite(P,FX)){UE_LOG(LogTemp,Error,TEXT("GAS SIM unavailable"));FX->DestroyComponent();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return 7;}check(Cache->WriteFrame(FX));Cache->EndWrite();
  TArray<FVector> Positions;TArray<FVector2D> Sizes;Cache->ReadPositionAttribute(Positions,TEXT("Position"),System->GetEmitterHandles()[0].GetName());Cache->ReadVector2Attribute(Sizes,TEXT("SpriteSize"),System->GetEmitterHandles()[0].GetName());
  TArray<double> Radii;for(int I=0;I<Positions.Num();++I)Radii.Add(Positions[I].Size2D()+(Sizes.IsValidIndex(I)?Sizes[I].GetMax()*.5:0));Radii.Sort();
  const double P90=Radii.Num()?Radii[FMath::Min(Radii.Num()-1,FMath::FloorToInt(Radii.Num()*.9))]:0;
  UE_LOG(LogTemp,Display,TEXT("GAS SAMPLE age=%.2f particles=%d p90Radius=%.3f maxRadius=%.3f"),Step/60.f,Radii.Num(),P90,Radii.Num()?Radii.Last():0);
 }
 FX->DestroyComponent();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return 0;
}
UVTGBossHealthBarComponent* HealthComponent(UBlueprint* BP,bool Write)
{
 for(UClass* C=BP->GeneratedClass;C;C=C->GetSuperClass())if(auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);B && B->SimpleConstructionScript)
  for(auto* N:B->SimpleConstructionScript->GetAllNodes())if(N->ComponentClass->IsChildOf(UVTGBossHealthBarComponent::StaticClass()))
  {
   if(!Write || B==BP)return CastChecked<UVTGBossHealthBarComponent>(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)));
   auto* H=BP->GetInheritableComponentHandler(true);FComponentKey Key(N);auto* O=H->GetOverridenComponentTemplate(Key);return CastChecked<UVTGBossHealthBarComponent>(O?O:H->CreateOverridenComponentTemplate(Key));
  }
 checkNoEntry();return nullptr;
}
bool Configure(bool Apply,float ReferenceRadius)
{
 auto* M=Material(Apply);check(M->MaterialDomain==MD_UI && M->BlendMode==BLEND_Translucent);
 auto* System=Gas(Apply);check(System->GetEmitterHandles().Num()>0);for(auto& H:System->GetEmitterHandles())check(H.GetEmitterData()->bLocalSpace);
 auto* BP=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster_L3Sewer"));
 if(Apply)
 {
  Backup(BP);auto* HUD=HealthComponent(BP,true);HUD->CausticOverlayMaterial=M;HUD->CausticOverlayOpacity=.65;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);check(!Log.NumErrors);Save(BP);
 }
 auto* HUD=HealthComponent(BP,false);check(HUD->CausticOverlayMaterial==M && FMath::IsNearlyEqual(HUD->CausticOverlayOpacity,.65f));
 auto* Montage=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_ToxicGas"));
 if(Apply)Backup(Montage);
 int Effects=0,DamageWindows=0;
 for(auto& E:Montage->Notifies)
 {
  if(auto* N=Cast<UVTGAttackEffectNotify>(E.Notify))
  {
   ++Effects;
   if(Apply){N->Effect=System;N->MediumEffect=nullptr;N->LargeEffect=nullptr;N->EffectPlayRate=2.5;N->EffectScale=1;N->EffectReferenceRadius=ReferenceRadius;N->ActiveSeconds=2.5;E.Link(Montage,.6);}
   check(N->Effect==System && FMath::IsNearlyEqual(N->EffectPlayRate,2.5f) && N->EffectReferenceRadius>0 && FMath::IsNearlyEqual(E.GetTime(),.6f));
   UE_LOG(LogTemp,Display,TEXT("BOSS SYNC FX onset=%.3f rate=%.2f referenceRadius=%.3f"),E.GetTime(),N->EffectPlayRate,N->EffectReferenceRadius);
  }
  if(E.NotifyStateClass && E.NotifyStateClass->GetClass()->GetName()==TEXT("Notify_Damage_C"))
  {
   ++DamageWindows;if(Apply){E.Link(Montage,1.1);E.SetDuration(.3);E.EndLink.Link(Montage,1.4);}
   check(FMath::IsNearlyEqual(E.GetTime(),1.1f,1.e-4f) && FMath::IsNearlyEqual(E.GetDuration(),.3f,1.e-4f));
   UE_LOG(LogTemp,Display,TEXT("BOSS SYNC DAMAGE onset=%.3f end=%.3f toolkitNotify=%s"),E.GetTime(),E.GetTime()+E.GetDuration(),*E.NotifyStateClass->GetClass()->GetPathName());
  }
 }
 check(Effects==1 && DamageWindows==1);
 if(Apply){Montage->SortNotifies();Montage->PostEditChange();Save(Montage);}
 UE_LOG(LogTemp,Display,TEXT("BOSS SYNC PASS: UI caustic overlay, BossAI area damage delayed by 0.5 seconds after VFX; all other notifies and damage settings retained"));return true;
}
}

UVTGSewerBossSyncCommandlet::UVTGSewerBossSyncCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSewerBossSyncCommandlet::Main(const FString& Params)
{
 using namespace BossSync;
 if(FParse::Param(*Params,TEXT("MaterialOnly"))){Material(true);return 0;}
 if(FParse::Param(*Params,TEXT("Sample")))return Sample(FParse::Param(*Params,TEXT("PrepareGas"))?Gas(true):Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Large")));
 if(FParse::Param(*Params,TEXT("Apply")) || FParse::Param(*Params,TEXT("VerifyOnly")))
 {
  float Radius=0;FParse::Value(*Params,TEXT("ReferenceRadius="),Radius);
  const bool Apply=FParse::Param(*Params,TEXT("Apply"));if(Apply && Radius<=0)return 2;
  return Configure(Apply,Radius)?0:1;
 }
 auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases"));check(Table);
 FFileHelper::SaveStringToFile(Table->GetTableAsJSON(),*(FPaths::ProjectDir()/TEXT("Scripts/SewerBossSync/abilities.json")));
 auto* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Large"));check(System);
 System->GetOutermost()->FullyLoad();System->ConditionalPostLoad();System->WaitForCompilationComplete();
 UE_LOG(LogTemp,Display,TEXT("GAS SYSTEM %s emitters=%d"),*System->GetPathName(),System->GetEmitterHandles().Num());
 for(auto& Handle:System->GetEmitterHandles())
 {
  auto* Data=Handle.GetEmitterData();check(Data);
  UE_LOG(LogTemp,Display,TEXT("GAS EMITTER %s local=%d sim=%d"),*Handle.GetName().ToString(),Data->bLocalSpace,int(Data->SimTarget));
  for(auto* Script:{Data->SpawnScriptProps.Script.Get(),Data->UpdateScriptProps.Script.Get(),Data->EmitterSpawnScriptProps.Script.Get(),Data->EmitterUpdateScriptProps.Script.Get()})if(Script)
  {
   TArray<FNiagaraVariable> Vars;Script->RapidIterationParameters.GetParameters(Vars);
   for(auto& V:Vars)
   {
    if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef()){UE_LOG(LogTemp,Display,TEXT("GAS PARAM %s=%f"),*V.GetName().ToString(),Script->RapidIterationParameters.GetParameterValue<float>(V));}
    if(V.GetType()==FNiagaraTypeDefinition::GetVec3Def()){UE_LOG(LogTemp,Display,TEXT("GAS PARAM %s=%s"),*V.GetName().ToString(),*FVector(Script->RapidIterationParameters.GetParameterValue<FVector3f>(V)).ToString());}
   }
  }
 }
 return 0;
}
