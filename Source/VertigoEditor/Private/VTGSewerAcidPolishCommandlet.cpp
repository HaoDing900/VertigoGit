#include "VTGSewerAcidPolishCommandlet.h"
#include "Combat/VTGCombatPresentation.h"
#include "Combat/VTGSewerBossPresentation.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/DataTable.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "RenderingThread.h"
#include "TimerManager.h"

namespace AcidPolish
{
const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
template<class T>T* Load(const FString& P){auto* O=LoadObject<T>(nullptr,*P);check(O);return O;}
FProperty* Field(const UStruct* S,const FString& N){for(TFieldIterator<FProperty> It(S);It;++It)if(It->GetName()==N || It->GetName().StartsWith(N+TEXT("_")))return *It;checkNoEntry();return nullptr;}
void Backup(UObject* O){const auto F=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const auto T=FPaths::ProjectDir()/TEXT("Scripts/SewerAcidPolish/Backup")/FPaths::GetCleanFilename(F);IFileManager::Get().MakeDirectory(*FPaths::GetPath(T),true);if(IFileManager::Get().FileExists(*F) && !IFileManager::Get().FileExists(*T))check(IFileManager::Get().Copy(*T,*F)==COPY_OK);}
void Save(UObject* O){auto* P=O->GetOutermost();P->FullyLoad();ResetLoaders(P);P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));}
void Compile(UBlueprint* B){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);FCompilerResultsLog L;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::SkipSave,&L);check(!L.NumErrors);}
template<class T>T* Component(UBlueprint* B,FName Name,bool Write)
{
 for(UClass* C=B->GeneratedClass;C;C=C->GetSuperClass())if(auto* P=Cast<UBlueprint>(C->ClassGeneratedBy);P && P->SimpleConstructionScript)for(auto* N:P->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==Name)
 {if(P==B || !Write)return CastChecked<T>(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(B->GeneratedClass)));auto* H=B->GetInheritableComponentHandler(true);FComponentKey K(N);auto* R=H->GetOverridenComponentTemplate(K);return CastChecked<T>(R?R:H->CreateOverridenComponentTemplate(K));}
 checkNoEntry();return nullptr;
}
void ClearHitReacts(UStruct* Type,void* Data)
{auto* P=CastFieldChecked<FStructProperty>(Field(Type,TEXT("HitReacts")));void* D=P->ContainerPtrToValuePtr<void>(Data);for(TFieldIterator<FObjectPropertyBase> It(P->Struct);It;++It)It->SetObjectPropertyValue_InContainer(D,nullptr);}
void Table(bool Apply)
{
 auto* T=Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"));auto* Row=T->FindRowUnchecked(TEXT("SeaMonster_P1"));check(Row);auto* P=CastFieldChecked<FArrayProperty>(Field(T->GetRowStruct(),TEXT("Abilities")));UScriptStruct* AT=CastFieldChecked<FStructProperty>(P->Inner)->Struct;FScriptArrayHelper Items(P,P->ContainerPtrToValuePtr<void>(Row));
 if(Apply){Backup(T);FFileHelper::SaveStringToFile(T->GetTableAsJSON(),*(FPaths::ProjectDir()/TEXT("Scripts/SewerAcidPolish/abilities-before.json")));for(int32 I=Items.Num()-1;I>=0;--I)if(CastFieldChecked<FNameProperty>(Field(AT,TEXT("AbilityName")))->GetPropertyValue_InContainer(Items.GetRawPtr(I))==TEXT("Headbutt"))Items.RemoveValues(I);ClearHitReacts(const_cast<UScriptStruct*>(T->GetRowStruct()),Row);T->PostEditChange();Save(T);}
 check(Items.Num()==3);for(int32 I=0;I<Items.Num();++I){auto Name=CastFieldChecked<FNameProperty>(Field(AT,TEXT("AbilityName")))->GetPropertyValue_InContainer(Items.GetRawPtr(I));check(Name!=TEXT("Headbutt"));UE_LOG(LogTemp,Display,TEXT("ACTIVE BOSS ABILITY %s"),*Name.ToString());}
}
void Projectile(bool Apply)
{
 const FString Name=TEXT("NS_AcidSpit_Muzzle");auto* System=LoadObject<UNiagaraSystem>(nullptr,*(Root+TEXT("FX/")+Name));
 if(Apply)
 {
  if(!System)System=CastChecked<UNiagaraSystem>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(Name,Root+TEXT("FX"),Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst"))));
  for(auto& H:System->GetEmitterHandles())
  {auto* D=H.GetEmitterData();D->bLocalSpace=true;for(auto* Script:{D->SpawnScriptProps.Script.Get(),D->UpdateScriptProps.Script.Get(),D->EmitterSpawnScriptProps.Script.Get(),D->EmitterUpdateScriptProps.Script.Get()})if(Script)
   {TArray<FNiagaraVariable> Vars;Script->RapidIterationParameters.GetParameters(Vars);for(auto& V:Vars)if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef()){const auto N=V.GetName().ToString();if(N.Contains(TEXT("InitializeParticle.Lifetime")))Script->RapidIterationParameters.SetParameterValue<float>(.45f,V);if(N.Contains(TEXT("EmitterState.Loop Delay")))Script->RapidIterationParameters.SetParameterValue<float>(0,V);}}}
  System->RequestCompile(false);System->WaitForCompilationComplete();Save(System);
 }
 check(System);auto* B=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));
 auto* Material=Load<UMaterialInstanceConstant>(TEXT("/Game/Resource/VFX/BloodPack/_Commons/Materials/MI_BloodDirection_07_Green"));
 auto* Impact=Load<UNiagaraSystem>(TEXT("/Game/Resource/VFX/BloodPack/Hit/Niagara/NS_BulletHit_GreenErosion"));
 if(Apply)
 {Backup(B);auto* FX=Component<UNiagaraComponent>(B,TEXT("AcidVisual"),true);FX->SetAutoActivate(false);FX->SetAsset(nullptr);
  Component<UStaticMeshComponent>(B,TEXT("VisibleAcidCore"),true)->SetMaterial(0,Material);
  auto* Flight=Component<UVTGAcidFlightComponent>(B,TEXT("AcidFlight"),true);Flight->MuzzleEffect=System;Flight->MuzzleScale=1;Flight->ImpactEffect=Impact;Flight->ImpactScale=1;
  Compile(B);Save(B);}
 auto* Flight=Component<UVTGAcidFlightComponent>(B,TEXT("AcidFlight"),false);check(Flight->MuzzleEffect==System && Flight->ImpactEffect==Impact);check(!Component<UNiagaraComponent>(B,TEXT("AcidVisual"),false)->GetAsset());check(Component<UStaticMeshComponent>(B,TEXT("VisibleAcidCore"),false)->GetMaterial(0)==Material);
}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->NodePosX=X;N->NodePosY=Y;N->NodeComment=TEXT("VTG boss upper body hit blend before anchor correction");return N;}
void Link(UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
void HitBlend(bool Apply)
{
 auto* Montage=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_GotHit"));auto* Skeleton=Montage->GetSkeleton();auto* Anim=Load<UAnimBlueprint>(Root+TEXT("ABP_EggProducer_L3Anchored"));
 const FName Slot(TEXT("BossHitReact"));
 if(Apply)
 {
  Backup(Montage);Backup(Skeleton);Backup(Anim);Skeleton->RegisterSlotNode(Slot);Skeleton->SetSlotGroupName(Slot,TEXT("BossHitReactGroup"));Save(Skeleton);
  for(auto& Track:Montage->SlotAnimTracks)Track.SlotName=Slot;Montage->BlendIn.SetBlendTime(.08f);Montage->BlendOut.SetBlendTime(.12f);Montage->bEnableAutoBlendOut=true;Montage->PostEditChange();Save(Montage);
  UEdGraph* Graph=nullptr;for(UEdGraph* G:Anim->FunctionGraphs)if(G->GetFName()==TEXT("AnimGraph"))Graph=G;check(Graph);bool Installed=false;UAnimGraphNode_LocalToComponentSpace* Convert=nullptr;
  for(UEdGraphNode* N:Graph->Nodes){if(auto* S=Cast<UAnimGraphNode_Slot>(N);S && S->Node.SlotName==Slot)Installed=true;if(auto* C=Cast<UAnimGraphNode_LocalToComponentSpace>(N))Convert=C;}
  if(!Installed)
  {
   check(Convert);auto* In=Convert->FindPinChecked(TEXT("LocalPose"));check(In->LinkedTo.Num()==1);auto* Original=In->LinkedTo[0];In->BreakAllPinLinks();
   auto* Cache=Add<UAnimGraphNode_SaveCachedPose>(Graph,-300,400);Cache->CacheName=TEXT("BossBaseBeforeHit");Cache->AllocateDefaultPins();Link(Original,Cache->FindPinChecked(TEXT("Pose")));
   auto MakeUse=[&](int Y){auto* Use=Add<UAnimGraphNode_UseCachedPose>(Graph,-100,Y);Use->SaveCachedPoseNode=Cache;Use->Node.CachePoseName=TEXT("BossBaseBeforeHit");Use->AllocateDefaultPins();return Use;};
   auto* Base=MakeUse(500);auto* Upper=MakeUse(700);auto* HitSlot=Add<UAnimGraphNode_Slot>(Graph,150,700);HitSlot->Node.SlotName=Slot;HitSlot->AllocateDefaultPins();Link(Upper->FindPinChecked(TEXT("Pose")),HitSlot->FindPinChecked(TEXT("Source")));
   auto* Blend=Add<UAnimGraphNode_LayeredBoneBlend>(Graph,450,500);Blend->Node.BlendPoses.SetNum(1);Blend->Node.BlendWeights.SetNum(1);Blend->Node.BlendWeights[0]=1;Blend->Node.LayerSetup.SetNum(1);FBranchFilter Filter;Filter.BoneName=TEXT("spine_001");Filter.BlendDepth=1;Blend->Node.LayerSetup[0].BranchFilters.Add(Filter);Blend->Node.bMeshSpaceRotationBlend=true;Blend->AllocateDefaultPins();Link(Base->FindPinChecked(TEXT("Pose")),Blend->FindPinChecked(TEXT("BasePose")));Link(HitSlot->FindPinChecked(TEXT("Pose")),Blend->FindPinChecked(TEXT("BlendPoses_0")));Link(Blend->FindPinChecked(TEXT("Pose")),In);
  }
  Compile(Anim);Save(Anim);
  auto* B=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster_L3Sewer"));Backup(B);auto* F=Component<UVTGEnemyFeedbackComponent>(B,TEXT("CombatFeedback"),true);F->HitEffect=Load<UNiagaraSystem>(TEXT("/Game/Resource/VFX/BloodPack/Hit/Niagara/NS_BulletHit_Cyan_Med1"));F->HitEffectSocketName=TEXT("spine_003");F->HitReactionMontage=Montage;F->HitReactionMinInterval=.2;auto* Behavior=Component<UActorComponent>(B,TEXT("Boss Behavior"),true);ClearHitReacts(Behavior->GetClass(),Behavior);Compile(B);Save(B);
 }
 check(Montage->SlotAnimTracks[0].SlotName==Slot && Skeleton->GetSlotGroupName(Slot)==TEXT("BossHitReactGroup"));auto* B=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster_L3Sewer"));check(Component<UVTGEnemyFeedbackComponent>(B,TEXT("CombatFeedback"),false)->HitReactionMontage==Montage);
}
void Hammer(bool Apply)
{
 const FString Folder=TEXT("/Game/Characters/Sa/Anm/Sa_Hammer/Combo/");auto* Third=Load<UAnimMontage>(Folder+TEXT("Hammer_Attack_Infinite_1_Montage"));
 if(Apply){Backup(Third);Third->RateScale=1.3f;Third->PostEditChange();Save(Third);}check(FMath::IsNearlyEqual(Third->RateScale,1.3f));
 for(const TCHAR* Name:{TEXT("Hammer_Attack_2Combo_1_Root_Montage"),TEXT("Hammer_Attack_2Combo_2_Root_Montage")})check(FMath::IsNearlyEqual(Load<UAnimMontage>(Folder+Name)->RateScale,1.f));
 UE_LOG(LogTemp,Display,TEXT("HAMMER third montage rate=1.3; first two unchanged=1"));
}
void Runtime()
{
 TGuardValue<bool> Execution(GAllowActorScriptExecutionInEditor,true);TGuardValue<bool> Client(GIsClient,true);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("AcidPolishTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);{TGuardValue<bool> Rendering(PRIVATE_GIsRunningCommandlet,false);World->CreateFXSystem();}FlushRenderingCommands();World->InitializeActorsForPlay(FURL());
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Player=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform(FVector(500,0,100)),P);auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Player);
 auto* ProjectileBP=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));auto* ProjectileActor=World->SpawnActor<AActor>(ProjectileBP->GeneratedClass,FTransform(FVector(0,0,100)),P);auto* Flight=ProjectileActor->FindComponentByClass<UVTGAcidFlightComponent>();check(Flight);Flight->BeginPlay();
 auto* Move=ProjectileActor->FindComponentByClass<UProjectileMovementComponent>();check(Move);const auto Direction=FVector(1,.4,-.2).GetSafeNormal();Move->Velocity=Direction*850;
 Flight->TickComponent(.016f,LEVELTICK_All,nullptr);check(Flight->MuzzleInstance);check(FVector::DotProduct(Flight->MuzzleInstance->GetUpVector(),Direction)>.999);const auto MuzzleLocation=Flight->MuzzleInstance->GetComponentLocation();auto* FirstMuzzle=Flight->MuzzleInstance.Get();ProjectileActor->SetActorLocation(FVector(100,0,100));Flight->TickComponent(.016f,LEVELTICK_All,nullptr);check(Flight->MuzzleInstance==FirstMuzzle && Flight->MuzzleInstance->GetComponentLocation().Equals(MuzzleLocation,.01));
 auto CountGreen=[&](){int32 N=0;for(TObjectIterator<UNiagaraComponent> It;It;++It)if(It->GetWorld()==World && It->GetAsset()==Flight->ImpactEffect && It->IsActive())++N;return N;};
 const int32 Before=CountGreen();Player->OnTakeAnyDamage.Broadcast(Player,5,nullptr,nullptr,Player);check(CountGreen()==Before);Player->OnTakeAnyDamage.Broadcast(Player,5,nullptr,nullptr,ProjectileActor);check(CountGreen()==Before+1);Player->OnTakeAnyDamage.Broadcast(Player,5,nullptr,nullptr,ProjectileActor);check(CountGreen()==Before+1);
 TWeakObjectPtr<UNiagaraComponent> Muzzle=Flight->MuzzleInstance;World->GetTimerManager().Tick(0);++GFrameCounter;World->GetTimerManager().Tick(.7);check(!Muzzle.IsValid());UE_LOG(LogTemp,Display,TEXT("ACID RUNTIME PASS: muzzle +Z follows actual projectile velocity, stays at launch position, emits once and cleans up; green erosion requires this projectile's damage"));
 auto* BossBP=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster_L3Sewer"));auto* Boss=World->SpawnActor<ACharacter>(BossBP->GeneratedClass,FTransform(FVector(1500,0,100)),P);auto* Mesh=Boss->GetMesh();Mesh->InitAnim(true);auto* Anim=Mesh->GetAnimInstance();check(Anim);auto* Hit=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_GotHit"));auto* Attack=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_AcidSpit"));check(Anim->Montage_Play(Attack)>0);
 auto* Feedback=Boss->FindComponentByClass<UVTGEnemyFeedbackComponent>();check(Feedback);static_cast<UActorComponent*>(Feedback)->BeginPlay();for(auto* C:Boss->GetComponents())if(auto* Health=FindFProperty<FNumericProperty>(C->GetClass(),TEXT("Health"))){auto* V=Health->ContainerPtrToValuePtr<void>(C);Health->SetFloatingPointPropertyValue(V,Health->GetFloatingPointPropertyValue(V)-1);}
 static_cast<UActorComponent*>(Feedback)->TickComponent(.016,LEVELTICK_All,nullptr);check(Anim->Montage_IsActive(Attack) && Anim->Montage_IsActive(Hit));Mesh->TickAnimation(.1,false);Mesh->RefreshBoneTransforms();check(!Mesh->GetBoneTransform(0).ContainsNaN());UE_LOG(LogTemp,Display,TEXT("BOSS HIT RUNTIME PASS: nonlethal damage starts separate hit montage while attack montage remains active; layered pose evaluates"));
 auto* PlayerBP=Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));auto* Defaults=CastChecked<ACharacter>(PlayerBP->GeneratedClass->GetDefaultObject());auto* Sa=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform(FVector(3000,0,100)),P);Sa->GetMesh()->SetSkeletalMesh(Defaults->GetMesh()->GetSkeletalMeshAsset());Sa->GetMesh()->SetAnimInstanceClass(Defaults->GetMesh()->GetAnimClass());Sa->GetMesh()->InitAnim(true);
 auto* Third=Load<UAnimMontage>(TEXT("/Game/Characters/Sa/Anm/Sa_Hammer/Combo/Hammer_Attack_Infinite_1_Montage"));auto* SaAnim=Sa->GetMesh()->GetAnimInstance();check(SaAnim && SaAnim->Montage_Play(Third)>0);Sa->GetMesh()->TickAnimation(.1,false);check(FMath::IsNearlyEqual(SaAnim->Montage_GetPosition(Third),.13f,.005f));UE_LOG(LogTemp,Display,TEXT("HAMMER RUNTIME PASS: third swing advanced %.3f animation seconds in 0.1 seconds"),SaAnim->Montage_GetPosition(Third));
 GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
}
}
UVTGSewerAcidPolishCommandlet::UVTGSewerAcidPolishCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSewerAcidPolishCommandlet::Main(const FString& Params){if(FParse::Param(*Params,TEXT("Runtime"))){AcidPolish::Runtime();return 0;}bool Apply=FParse::Param(*Params,TEXT("Apply"));AcidPolish::Table(Apply);AcidPolish::Projectile(Apply);AcidPolish::HitBlend(Apply);AcidPolish::Hammer(Apply);UE_LOG(LogTemp,Display,TEXT("ACID POLISH %s PASS"),Apply?TEXT("APPLY"):TEXT("VERIFY"));return 0;}
