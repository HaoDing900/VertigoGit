#include "VTGSewerGasLifetimeCommandlet.h"
#include "Combat/VTGCombatPresentation.h"
#include "Combat/VTGSmokeHazard.h"
#include "Combat/VTGSewerBossPresentation.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "Components/StaticMeshComponent.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraSimCache.h"
#include "RenderingThread.h"
#include "UObject/StructOnScope.h"

namespace GasSetup
{
const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
template<class T>T* Load(const FString& P){auto* O=LoadObject<T>(nullptr,*P);check(O);return O;}
FProperty* Field(const UStruct* S,const FString& N){for(TFieldIterator<FProperty> It(S);It;++It)if(It->GetName()==N || It->GetName().StartsWith(N+TEXT("_")))return *It;checkNoEntry();return nullptr;}
void Number(UStruct* S,void* O,const FString& N,double V){auto* P=CastFieldChecked<FNumericProperty>(Field(S,N));auto* D=P->ContainerPtrToValuePtr<void>(O);if(P->IsInteger())P->SetIntPropertyValue(D,int64(V));else P->SetFloatingPointPropertyValue(D,V);}
void* Struct(UStruct* S,void* O,const FString& N,UScriptStruct*& T){auto* P=CastFieldChecked<FStructProperty>(Field(S,N));T=P->Struct;return P->ContainerPtrToValuePtr<void>(O);}
void Backup(UObject* O){const auto F=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const auto T=FPaths::ProjectDir()/TEXT("Scripts/SewerGasLifetime/Backup")/FPaths::GetCleanFilename(F);IFileManager::Get().MakeDirectory(*FPaths::GetPath(T),true);if(IFileManager::Get().FileExists(*F) && !IFileManager::Get().FileExists(*T))check(IFileManager::Get().Copy(*T,*F)==COPY_OK);}
void Save(UObject* O){auto* P=O->GetOutermost();P->FullyLoad();ResetLoaders(P);P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));}
void Compile(UBlueprint* B){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);FCompilerResultsLog L;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::SkipSave,&L);check(!L.NumErrors);}
template<class T>T* Component(UBlueprint* B,FName Name,bool Write)
{
 for(UClass* C=B->GeneratedClass;C;C=C->GetSuperClass())if(auto* P=Cast<UBlueprint>(C->ClassGeneratedBy);P && P->SimpleConstructionScript)for(auto* N:P->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==Name)
 {if(P==B || !Write)return CastChecked<T>(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(B->GeneratedClass)));auto* H=B->GetInheritableComponentHandler(true);FComponentKey K(N);auto* R=H->GetOverridenComponentTemplate(K);return CastChecked<T>(R?R:H->CreateOverridenComponentTemplate(K));}
 check(Write);auto* N=B->SimpleConstructionScript->CreateNode(T::StaticClass(),Name);B->SimpleConstructionScript->AddNode(N);return CastChecked<T>(N->ComponentTemplate);
}
UNiagaraSystem* Cloud(bool Boss,bool Apply)
{
 const FString Name=Boss?TEXT("NS_Boss_ToxicGas_Lifetime"):TEXT("NS_Egg_ToxicGas_Lifetime");
 auto* S=LoadObject<UNiagaraSystem>(nullptr,*(Root+TEXT("FX/")+Name));
 if(Apply)
 {
  if(!S)S=CastChecked<UNiagaraSystem>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(Name,Root+TEXT("FX"),Load<UNiagaraSystem>(Boss?Root+TEXT("FX/NS_Boss_ToxicGas"):TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Mid"))));
  int32 Lifetimes=0;for(auto& H:S->GetEmitterHandles())
  {
   auto* D=H.GetEmitterData();D->bLocalSpace=true;
   for(auto* Script:{D->SpawnScriptProps.Script.Get(),D->UpdateScriptProps.Script.Get(),D->EmitterSpawnScriptProps.Script.Get(),D->EmitterUpdateScriptProps.Script.Get()})if(Script)
   {TArray<FNiagaraVariable> Vars;Script->RapidIterationParameters.GetParameters(Vars);for(auto& V:Vars)if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef())
    {const auto N=V.GetName().ToString();if(N.Contains(TEXT("InitializeParticle.Lifetime"))){Script->RapidIterationParameters.SetParameterValue<float>(Boss?7.5f:3.f,V);++Lifetimes;}if(N.Contains(TEXT("EmitterState.Loop Delay")))Script->RapidIterationParameters.SetParameterValue<float>(0,V);}}
  }
  check(Lifetimes>0);S->RequestCompile(false);S->WaitForCompilationComplete();Save(S);
 }
 check(S);for(auto& H:S->GetEmitterHandles())for(auto* Script:{H.GetEmitterData()->SpawnScriptProps.Script.Get(),H.GetEmitterData()->UpdateScriptProps.Script.Get()})if(Script)
 {TArray<FNiagaraVariable> Vars;Script->RapidIterationParameters.GetParameters(Vars);for(auto& V:Vars)if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef() && V.GetName().ToString().Contains(TEXT("InitializeParticle.Lifetime")))check(FMath::IsNearlyEqual(Script->RapidIterationParameters.GetParameterValue<float>(V),Boss?7.5f:3.f));}
 return S;
}
void Montage(bool Boss,bool Apply)
{
 auto* M=Load<UAnimMontage>(Root+TEXT("Montages/")+(Boss?TEXT("AM_EggProducer_ToxicGas"):TEXT("AM_SeaMonsterEgg_ReleaseGas")));if(Apply)Backup(M);
 auto* System=Cloud(Boss,Apply);int32 FXCount=0;
 if(Apply)M->Notifies.RemoveAll([](const FAnimNotifyEvent& E){return E.NotifyStateClass && E.NotifyStateClass->GetClass()->GetName()==TEXT("Notify_Damage_C");});
 for(auto& N:M->Notifies)
 {
  check(!N.NotifyStateClass || N.NotifyStateClass->GetClass()->GetName()!=TEXT("Notify_Damage_C"));
  if(auto* FX=Cast<UVTGAttackEffectNotify>(N.Notify))
  {
   ++FXCount;if(Apply){FX->Effect=System;FX->bPersistentSmokeDamage=true;FX->SmokeLifetime=3;FX->SmokeSafeFadeSeconds=.6;FX->SmokeDamageDelay=Boss?.5f:.3f;FX->SmokeTickInterval=.5;FX->SmokeTickMultiplier=Boss?.25f:.15f;FX->EffectPlayRate=Boss?2.5f:1.f;N.Link(M,Boss?1.f:.8f);}
   check(FX->Effect==System && FX->bPersistentSmokeDamage && FMath::IsNearlyEqual(FX->SmokeLifetime,3.f) && FMath::IsNearlyEqual(FX->SmokeSafeFadeSeconds,.6f));
   check(FMath::IsNearlyEqual(FX->SmokeDamageDelay,Boss?.5f:.3f));
   UE_LOG(LogTemp,Display,TEXT("GAS %s FX=%.2f first hit=%.2f stop=%.2f particles end=%.2f"),Boss?TEXT("BOSS"):TEXT("EGG"),N.GetTime(),N.GetTime()+FX->SmokeDamageDelay,N.GetTime()+FX->SmokeLifetime-FX->SmokeSafeFadeSeconds,N.GetTime()+FX->SmokeLifetime);
  }
 }
 check(FXCount==1);if(Apply){M->SortNotifies();M->PostEditChange();Save(M);}
}
template<class T>T* Node(UEdGraph* G){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodeComment=TEXT("VTG sewer smoke lifetime / egg spawn initialization");return N;}
void Link(UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
void Spawner(bool Apply)
{
 auto* B=Load<UBlueprint>(Root+TEXT("BP_Ability_SpawnEggs"));if(Apply)
 {
  Backup(B);UEdGraph* G=B->UbergraphPages[0];UK2Node_Event* Begin=nullptr;UK2Node_CallFunction* Parent=nullptr;UK2Node_IfThenElse* Nav=nullptr;
  bool Installed=false;for(UEdGraphNode* N:G->Nodes)
  {if(auto* E=Cast<UK2Node_Event>(N);E && E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay"))Begin=E;
   if(auto* C=Cast<UK2Node_CallFunction>(N)){if(C->FunctionReference.GetMemberName()==TEXT("InitializeEggSpawner"))Installed=true;if(C->GetName()==TEXT("K2Node_CallParentFunction_0"))Parent=C;}
   if(auto* C=Cast<UK2Node_IfThenElse>(N);C && C->GetName()==TEXT("K2Node_IfThenElse_0"))Nav=C;}
  check(Begin && Parent && Nav);Begin->SetEnabledState(ENodeEnabledState::Enabled);Parent->SetEnabledState(ENodeEnabledState::Enabled);
  if(!Installed)
  {
   auto* Init=Node<UK2Node_CallFunction>(G);Init->SetFromFunction(UVTGSewerAbilityHelpers::StaticClass()->FindFunctionByName(TEXT("InitializeEggSpawner")));Init->AllocateDefaultPins();Init->NodePosX=Parent->NodePosX+450;Init->NodePosY=Parent->NodePosY;
   auto* Self=Node<UK2Node_Self>(G);Self->AllocateDefaultPins();Self->NodePosY=Init->NodePosY+200;
   Link(Parent->FindPinChecked(TEXT("then")),Init->FindPinChecked(TEXT("execute")));Link(Self->FindPinChecked(TEXT("self")),Init->FindPinChecked(TEXT("Ability")));
   auto* Fallback=Node<UK2Node_CallFunction>(G);Fallback->SetFromFunction(UVTGSewerAbilityHelpers::StaticClass()->FindFunctionByName(TEXT("SpawnEggWithoutNavigation")));Fallback->AllocateDefaultPins();Fallback->NodePosX=Nav->NodePosX+300;Fallback->NodePosY=Nav->NodePosY+400;
   Link(Nav->FindPinChecked(TEXT("else")),Fallback->FindPinChecked(TEXT("execute")));Link(Self->FindPinChecked(TEXT("self")),Fallback->FindPinChecked(TEXT("Ability")));
  }
  Compile(B);Number(B->GeneratedClass,B->GeneratedClass->GetDefaultObject(),TEXT("SpawnCount"),3);Number(B->GeneratedClass,B->GeneratedClass->GetDefaultObject(),TEXT("SpawnRadius"),600);Save(B);
 }
 auto* Count=FindFProperty<FIntProperty>(B->GeneratedClass,TEXT("SpawnCount"));check(Count && Count->GetPropertyValue_InContainer(B->GeneratedClass->GetDefaultObject())==3);
 auto* M=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_SpawnEggs"));int32 Executes=0;
 for(auto& N:M->Notifies)if(N.NotifyStateClass && N.NotifyStateClass->GetClass()->GetName()==TEXT("Notify_Execute_C"))++Executes;
 if(Apply && !Executes){Backup(M);UClass* Class=Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/Notifies/Notify_Execute"))->GeneratedClass;auto& N=M->Notifies.AddDefaulted_GetRef();N.NotifyStateClass=NewObject<UAnimNotifyState>(M,Class);N.NotifyName=TEXT("Notify_Execute");N.Guid=FGuid::NewGuid();N.Link(M,1.f);N.SetDuration(.1);N.EndLink.Link(M,1.1f);M->SortNotifies();M->PostEditChange();Save(M);Executes=1;}
 check(Executes>0);UE_LOG(LogTemp,Display,TEXT("SPAWN EGGS count=3 radius=600 execute windows=%d"),Executes);
}
void Table(bool Apply)
{
 auto* T=Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"));if(Apply)Backup(T);
 auto* Array=CastFieldChecked<FArrayProperty>(Field(T->GetRowStruct(),TEXT("Abilities")));UScriptStruct* AT=CastFieldChecked<FStructProperty>(Array->Inner)->Struct;
 for(bool Boss:{false,true})
 {
  FScriptArrayHelper Items(Array,Array->ContainerPtrToValuePtr<void>(T->FindRowUnchecked(Boss?TEXT("SeaMonster_P1"):TEXT("SeaMonsterEgg_P1"))));
  for(int32 I=0;I<Items.Num();++I)
  {
   void* A=Items.GetRawPtr(I);auto* P=CastFieldChecked<FNameProperty>(Field(AT,TEXT("AbilityName")));if(P->GetPropertyValue_InContainer(A)!=(Boss?TEXT("Toxic Gas"):TEXT("Gas Burst")))continue;
   UScriptStruct* DT=nullptr;void* D=Struct(AT,A,TEXT("Damage"),DT);
   if(Apply){Number(DT,D,TEXT("Duration"),0);Number(DT,D,TEXT("PerTickMultiplier"),0);CastFieldChecked<FClassProperty>(Field(DT,TEXT("DamageType")))->SetObjectPropertyValue_InContainer(D,Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default"))->GeneratedClass);
    if(Boss){Number(AT,A,TEXT("Cooldown"),10./.9);Number(AT,A,TEXT("Duration"),4);UScriptStruct* VT=nullptr;void* V=Struct(AT,A,TEXT("Visuals"),VT);CastFieldChecked<FBoolProperty>(Field(VT,TEXT("ForceShowTelegraph")))->SetPropertyValue_InContainer(V,true);}}
   check(CastFieldChecked<FNumericProperty>(Field(DT,TEXT("Duration")))->GetFloatingPointPropertyValue(Field(DT,TEXT("Duration"))->ContainerPtrToValuePtr<void>(D))==0);
  }
 }
 if(Apply){T->PostEditChange();Save(T);}FFileHelper::SaveStringToFile(T->GetTableAsJSON(),*(FPaths::ProjectDir()/TEXT("Scripts/SewerGasLifetime/abilities.json")));
}
void Projectile(bool Apply)
{
 auto* B=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));if(Apply)
 {
  Backup(B);auto* FX=Component<UNiagaraComponent>(B,TEXT("AcidVisual"),true);FX->SetRelativeScale3D(FVector(1.2));
  const FString MatPath=Root+TEXT("FX/M_AcidProjectile_Visible");auto* Mat=LoadObject<UMaterial>(nullptr,*MatPath);
  if(!Mat){Mat=NewObject<UMaterial>(CreatePackage(*MatPath),TEXT("M_AcidProjectile_Visible"),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Mat);Mat->SetShadingModel(MSM_Unlit);auto* C=CastChecked<UMaterialExpressionConstant3Vector>(UMaterialEditingLibrary::CreateMaterialExpression(Mat,UMaterialExpressionConstant3Vector::StaticClass()));C->Constant=FLinearColor(.8,5,.25);UMaterialEditingLibrary::ConnectMaterialProperty(C,TEXT(""),MP_EmissiveColor);UMaterialEditingLibrary::RecompileMaterial(Mat);Save(Mat);}
  auto* Core=Component<UStaticMeshComponent>(B,TEXT("VisibleAcidCore"),true);Core->SetStaticMesh(Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere")));Core->SetMaterial(0,Mat);Core->SetRelativeScale3D(FVector(.24));Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);Core->SetCastShadow(false);
  auto* Flight=Component<UVTGAcidFlightComponent>(B,TEXT("AcidFlight"),true);Flight->ImpactEffect=Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst"));Flight->ImpactScale=1.5;
  Compile(B);Save(B);
 }
 check(Component<UStaticMeshComponent>(B,TEXT("VisibleAcidCore"),false)->GetStaticMesh());check(Component<UVTGAcidFlightComponent>(B,TEXT("AcidFlight"),false)->ImpactEffect);UE_LOG(LogTemp,Display,TEXT("ACID projectile core=24cm Niagara scale=1.2 impact burst=1.5 (collision unchanged)"));
}
void Runtime()
{
 TGuardValue<bool> Execution(GAllowActorScriptExecutionInEditor,true);TGuardValue<bool> Client(GIsClient,true);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SmokeLifetimeTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 {TGuardValue<bool> Rendering(PRIVATE_GIsRunningCommandlet,false);World->CreateFXSystem();}FlushRenderingCommands();World->InitializeActorsForPlay(FURL());World->BeginPlay();
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Player=World->SpawnActor<AVTGSmokeDamageTestPawn>(AVTGSmokeDamageTestPawn::StaticClass(),FTransform(FVector(100,0,100)),Params);Player->Tags.Add(TEXT("Player"));auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Player);
 auto* Source=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform(FVector(0,0,100)),Params);
 auto* Behavior=NewObject<UActorComponent>(Source,Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/BPC_Boss_Behavior"))->GeneratedClass);Source->AddInstanceComponent(Behavior);Number(Behavior->GetClass(),Behavior,TEXT("AttackPower"),20);
 auto SetObject=[](UObject* O,const TCHAR* N,UObject* V){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),N);check(P);P->SetObjectPropertyValue_InContainer(O,V);};
 SetObject(Behavior,TEXT("ControllerRef"),PC);SetObject(Behavior,TEXT("CharacterRef"),Source);
 auto* Table=Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"));auto* AP=CastFieldChecked<FArrayProperty>(Field(Table->GetRowStruct(),TEXT("Abilities")));UScriptStruct* AT=CastFieldChecked<FStructProperty>(AP->Inner)->Struct;
 for(bool Boss:{false,true})
 {
  Player->ReceivedDamage=0;Player->SetActorLocation(FVector(100,0,100));
  FScriptArrayHelper Items(AP,AP->ContainerPtrToValuePtr<void>(Table->FindRowUnchecked(Boss?TEXT("SeaMonster_P1"):TEXT("SeaMonsterEgg_P1"))));void* Data=nullptr;
  for(int32 I=0;I<Items.Num();++I)if(CastFieldChecked<FNameProperty>(Field(AT,TEXT("AbilityName")))->GetPropertyValue_InContainer(Items.GetRawPtr(I))==(Boss?TEXT("Toxic Gas"):TEXT("Gas Burst")))Data=Items.GetRawPtr(I);check(Data);
  auto* Ability=World->SpawnActorDeferred<AActor>(Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Area"))->GeneratedClass,FTransform(FVector(0,0,100)),Source);
  auto* Info=FindFProperty<FStructProperty>(Ability->GetClass(),TEXT("AbilityInfo"));Info->CopyCompleteValue(Info->ContainerPtrToValuePtr<void>(Ability),Data);Ability->FinishSpawning(FTransform(FVector(0,0,100)));SetObject(Ability,TEXT("BossRef"),Source);SetObject(Ability,TEXT("BossBehaviorRef"),Behavior);
  auto* M=Load<UAnimMontage>(Root+TEXT("Montages/")+(Boss?TEXT("AM_EggProducer_ToxicGas"):TEXT("AM_SeaMonsterEgg_ReleaseGas")));UVTGAttackEffectNotify* Settings=nullptr;for(auto& E:M->Notifies)if(auto* N=Cast<UVTGAttackEffectNotify>(E.Notify))Settings=N;check(Settings);
  auto* H=AVTGSmokeHazard::Spawn(Source,Ability,Settings,Settings->Effect,FVector(0,0,100),1);check(H);H->Cloud->SetForceSolo(true);H->Cloud->SetAllowScalability(false);H->Cloud->Activate(true);
  float SafeDamage=-1,OutsideDamage=-1;int32 FadeParticles=0;
  for(int32 Step=0;Step<180 && !H->IsActorBeingDestroyed();++Step)
  {
   const float Age=Step/60.f;
   if(Step==45){OutsideDamage=Player->ReceivedDamage;Player->SetActorLocation(FVector(5000,0,100));}
   if(Step==75){check(FMath::IsNearlyEqual(Player->ReceivedDamage,OutsideDamage));Player->SetActorLocation(FVector(100,0,100));Ability->Destroy();}
   H->Cloud->AdvanceSimulation(1,Settings->EffectPlayRate/60.f);H->Tick(1.f/60);
   if(H->Age<Settings->SmokeDamageDelay-.02f)check(Player->ReceivedDamage==0);
   if(Step==145)
   {
    SafeDamage=Player->ReceivedDamage;check(SafeDamage>0);
    auto* Cache=NewObject<UNiagaraSimCache>();FNiagaraSimCacheCreateParameters CP;check(Cache->BeginWrite(CP,H->Cloud));check(Cache->WriteFrame(H->Cloud));Cache->EndWrite();TArray<FVector> Positions;Cache->ReadPositionAttribute(Positions,TEXT("Position"),Settings->Effect->GetEmitterHandles()[0].GetName());FadeParticles=Positions.Num();check(FadeParticles>0);
   }
   if(SafeDamage>=0)check(FMath::IsNearlyEqual(SafeDamage,Player->ReceivedDamage));
  }
  check(SafeDamage>OutsideDamage && FadeParticles>0);UE_LOG(LogTemp,Display,TEXT("SMOKE RUNTIME %s PASS damage=%.2f fade particles=%d; delay, exit/reenter, original ability destruction and safe fade verified"),Boss?TEXT("BOSS"):TEXT("EGG"),Player->ReceivedDamage,FadeParticles);if(!H->IsActorBeingDestroyed())H->Destroy();
 }
 // Execute the real spawner event without a NavMesh; the fallback must spawn onto a physical floor.
 auto* Floor=World->SpawnActor<AActor>();auto* Mesh=NewObject<UStaticMeshComponent>(Floor);Floor->SetRootComponent(Mesh);Floor->AddInstanceComponent(Mesh);Mesh->SetStaticMesh(Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube")));Mesh->SetWorldScale3D(FVector(100,100,.1));Mesh->SetWorldLocation(FVector(0,0,-5));Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->RegisterComponent();
 auto* Spawner=World->SpawnActorDeferred<AActor>(Load<UBlueprint>(Root+TEXT("BP_Ability_SpawnEggs"))->GeneratedClass,FTransform::Identity,Source);
 FScriptArrayHelper BossItems(AP,AP->ContainerPtrToValuePtr<void>(Table->FindRowUnchecked(TEXT("SeaMonster_P1"))));for(int32 I=0;I<BossItems.Num();++I)if(CastFieldChecked<FNameProperty>(Field(AT,TEXT("AbilityName")))->GetPropertyValue_InContainer(BossItems.GetRawPtr(I))==TEXT("Spawn Eggs")){auto* P=FindFProperty<FStructProperty>(Spawner->GetClass(),TEXT("AbilityInfo"));P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Spawner),BossItems.GetRawPtr(I));}
 Spawner->FinishSpawning(FTransform::Identity);if(!Spawner->HasActorBegunPlay())Spawner->DispatchBeginPlay();check(FindFProperty<FIntProperty>(Spawner->GetClass(),TEXT("SpawnCount"))->GetPropertyValue_InContainer(Spawner)==3);check(FindFProperty<FObjectPropertyBase>(Spawner->GetClass(),TEXT("SpawnClass"))->GetObjectPropertyValue_InContainer(Spawner));
 auto* F=Spawner->FindFunction(TEXT("OnExecute"));check(F);FStructOnScope Args(F);Spawner->ProcessEvent(F,Args.GetStructMemory());int32 Eggs=0;auto* EggClass=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_Egg"))->GeneratedClass.Get();for(TActorIterator<AActor> It(World,EggClass);It;++It)++Eggs;check(Eggs==3);UE_LOG(LogTemp,Display,TEXT("SPAWNER RUNTIME PASS: OnExecute generated %d eggs without a NavMesh"),Eggs);
 auto* ProjectileBP=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));auto* Impact=Component<UVTGAcidFlightComponent>(ProjectileBP,TEXT("AcidFlight"),false)->ImpactEffect.Get();
 auto CountImpacts=[&](){int32 N=0;for(TObjectIterator<UNiagaraComponent> It;It;++It)if(It->GetWorld()==World && It->GetAsset()==Impact && It->IsActive())++N;return N;};
 auto* ProjectileActor=World->SpawnActor<AActor>(ProjectileBP->GeneratedClass,FTransform(FVector(3000,0,100)),Params);auto* Flight=ProjectileActor->FindComponentByClass<UVTGAcidFlightComponent>();Flight->BeginPlay();
 for(auto* C:ProjectileActor->GetComponents())if(C->GetName().Contains(TEXT("VisibleAcidCore")))check(CastChecked<UStaticMeshComponent>(C)->GetComponentLocation().Equals(ProjectileActor->GetActorLocation(),.01));
 const int32 BeforeImpact=CountImpacts();ProjectileActor->Destroy();check(CountImpacts()==BeforeImpact+1);
 auto* RangeActor=World->SpawnActor<AActor>(ProjectileBP->GeneratedClass,FTransform(FVector(3000,0,100)),Params);auto* RangeFlight=RangeActor->FindComponentByClass<UVTGAcidFlightComponent>();RangeFlight->BeginPlay();const int32 BeforeRange=CountImpacts();RangeActor->SetActorLocation(FVector(5000,0,100));RangeFlight->TickComponent(.1,LEVELTICK_All,nullptr);check(CountImpacts()==BeforeRange && RangeActor->IsActorBeingDestroyed());UE_LOG(LogTemp,Display,TEXT("ACID RUNTIME PASS: visible core follows projectile, impact creates splash, range expiration creates no false impact"));
 GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
}
}
UVTGSewerGasLifetimeCommandlet::UVTGSewerGasLifetimeCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSewerGasLifetimeCommandlet::Main(const FString& Params){if(FParse::Param(*Params,TEXT("Runtime"))){GasSetup::Runtime();return 0;}bool Apply=FParse::Param(*Params,TEXT("Apply"));GasSetup::Table(Apply);GasSetup::Montage(false,Apply);GasSetup::Montage(true,Apply);GasSetup::Spawner(Apply);GasSetup::Projectile(Apply);UE_LOG(LogTemp,Display,TEXT("GAS LIFETIME %s PASS"),Apply?TEXT("APPLY"):TEXT("VERIFY"));return 0;}

