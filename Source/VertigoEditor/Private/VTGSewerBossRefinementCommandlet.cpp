#include "VTGSewerBossRefinementCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "Combat/VTGSewerBossPresentation.h"
#include "Combat/VTGCombatPresentation.h"
#include "UI/VTGBossHealthBarComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/StructOnScope.h"

namespace SewerRefine
{
template<class T>T* CheckedProperty(UStruct* S,FName Name){auto* P=FindFProperty<T>(S,Name);check(P);return P;}
const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
template<class T>T* Load(const FString& Path){auto* O=LoadObject<T>(nullptr,*Path);checkf(O,TEXT("Missing %s"),*Path);return O;}
void Backup(UObject* O)
{
 const FString File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString To=FPaths::ProjectDir()/TEXT("Scripts/SewerBossRefinement/Backup")/(O->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true);
 if(IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*To))check(IFileManager::Get().Copy(*To,*File)==COPY_OK);
}
void Save(UObject* O){auto* P=O->GetOutermost();P->FullyLoad();ResetLoaders(P);P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));}
void Compile(UBlueprint* BP){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);checkf(!Log.NumErrors,TEXT("Compile %s"),*BP->GetName());}
template<class T>T* Component(UBlueprint* BP,FName Name,bool Write)
{
 for(UClass* C=BP->GeneratedClass;C;C=C->GetSuperClass())if(auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);B && B->SimpleConstructionScript)
  for(auto* N:B->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==Name)
  {if(B==BP || !Write)return CastChecked<T>(N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)));auto* Handler=BP->GetInheritableComponentHandler(true);FComponentKey Key(N);auto* Result=Handler->GetOverridenComponentTemplate(Key);return CastChecked<T>(Result?Result:Handler->CreateOverridenComponentTemplate(Key));}
 check(Write);auto* N=BP->SimpleConstructionScript->CreateNode(T::StaticClass(),Name);BP->SimpleConstructionScript->AddNode(N);return CastChecked<T>(N->ComponentTemplate);
}
template<class T>T* Duplicate(T* Source,const FString& Name)
{
 if(auto* Existing=LoadObject<T>(nullptr,*(Root+Name)))return Existing;
 return CastChecked<T>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(Name,Root.LeftChop(1),Source));
}
void Egg()
{
 auto* BP=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_Egg"));Backup(BP);
 auto* Seq=Load<UAnimSequence>(TEXT("/Game/Characters/Monster/MonsterEgg/ANM/Monster_Egg1_Death"));
 auto* Montage=LoadObject<UAnimMontage>(nullptr,*(Root+TEXT("Montages/AM_SeaMonsterEgg_Death")));
 if(!Montage)
 {
  auto* Dynamic=UAnimMontage::CreateSlotAnimationAsDynamicMontage(Seq,TEXT("DefaultSlot"),.08f,0,1,1);
  const FString Path=Root+TEXT("Montages/AM_SeaMonsterEgg_Death");Montage=DuplicateObject<UAnimMontage>(Dynamic,CreatePackage(*Path),TEXT("AM_SeaMonsterEgg_Death"));Montage->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Montage);
 }
 Backup(Montage);Montage->bEnableAutoBlendOut=false;
 Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& E){return E.Notify && E.Notify->IsA<UVTGEggRagdollNotify>();});
 auto& Event=Montage->Notifies.AddDefaulted_GetRef();Event.Notify=NewObject<UVTGEggRagdollNotify>(Montage);Event.NotifyName=TEXT("VTG_DeathPoseToRagdoll");Event.Guid=FGuid::NewGuid();Event.Link(Montage,FMath::Max(0.f,Seq->GetPlayLength()-1.f/30));Montage->SortNotifies();Montage->PostEditChange();Save(Montage);
 auto* Behavior=Component<UActorComponent>(BP,TEXT("Boss Behavior"),true);
 CheckedProperty<FBoolProperty>(Behavior->GetClass(),TEXT("UseDeathAnim"))->SetPropertyValue_InContainer(Behavior,true);
 CheckedProperty<FObjectPropertyBase>(Behavior->GetClass(),TEXT("DeathAnim"))->SetObjectPropertyValue_InContainer(Behavior,Montage);
 auto* Mesh=CastChecked<ACharacter>(BP->GeneratedClass->GetDefaultObject())->GetMesh();auto* Physics=Duplicate(Mesh->GetPhysicsAsset(),TEXT("PA_SeaMonsterEgg_ScaledBones"));
 Backup(Physics);for(USkeletalBodySetup* Body:Physics->SkeletalBodySetups)Body->bSkipScaleFromAnimation=false;Physics->PostEditChange();Save(Physics);Mesh->SetPhysicsAsset(Physics);Compile(BP);Save(BP);
}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->NodeComment=TEXT("VTG L3 hanging boss");N->NodePosX=X;N->NodePosY=Y;return N;}
void Link(UEdGraphPin* A,UEdGraphPin* B){check(A && B);check(A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A,B));}
void AnchorBoss()
{
 auto* BP=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster_L3Sewer"));Backup(BP);Component<UVTGAnchoredBossComponent>(BP,TEXT("HangingBoss"),true);
 auto* Anim=Duplicate(Load<UAnimBlueprint>(Root+TEXT("ABP_EggProducer")),TEXT("ABP_EggProducer_L3Anchored"));Backup(Anim);Anim->ParentClass=UVTGAnchoredBossAnimInstance::StaticClass();Compile(Anim);
 UEdGraph* G=nullptr;for(UEdGraph* Graph:Anim->FunctionGraphs)if(Graph->GetFName()==TEXT("AnimGraph"))G=Graph;check(G);
 bool Done=false;for(UEdGraphNode* N:G->Nodes)Done|=N->NodeComment==TEXT("VTG L3 hanging boss");
 if(!Done)
 {
  UAnimGraphNode_Root* Output=nullptr;for(UEdGraphNode* N:G->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N))Output=R;check(Output);auto* In=Output->FindPinChecked(TEXT("Result"));check(In->LinkedTo.Num()==1);auto* Previous=In->LinkedTo[0];In->BreakAllPinLinks();
  auto* ToCS=Add<UAnimGraphNode_LocalToComponentSpace>(G,0,0);ToCS->AllocateDefaultPins();Link(Previous,ToCS->FindPinChecked(TEXT("LocalPose")));Previous=ToCS->FindPinChecked(TEXT("ComponentPose"));
  const auto& Ref=Anim->TargetSkeleton->GetReferenceSkeleton();
  for(const TCHAR* Name:{TEXT("root"),TEXT("spine")})
  {
   int Index=Ref.FindBoneIndex(Name);check(Index!=INDEX_NONE);FTransform Transform=Ref.GetRefBonePose()[Index];for(int Parent=Ref.GetParentIndex(Index);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent))Transform*=Ref.GetRefBonePose()[Parent];
   auto* Bone=Add<UAnimGraphNode_ModifyBone>(G,300+Index*300,0);Bone->Node.BoneToModify.BoneName=Name;Bone->Node.TranslationMode=BMM_Replace;Bone->Node.TranslationSpace=BCS_ComponentSpace;Bone->Node.Translation=Transform.GetLocation();Bone->Node.RotationMode=BMM_Replace;Bone->Node.RotationSpace=BCS_ComponentSpace;Bone->Node.Rotation=Transform.Rotator();Bone->Node.ScaleMode=BMM_Ignore;Bone->AllocateDefaultPins();Link(Previous,Bone->FindPinChecked(TEXT("ComponentPose")));Previous=Bone->FindPinChecked(TEXT("Pose"));
  }
  auto* Aim=Add<UAnimGraphNode_ModifyBone>(G,900,0);Aim->Node.BoneToModify.BoneName=TEXT("spine_001");Aim->Node.RotationMode=BMM_Additive;Aim->Node.RotationSpace=BCS_WorldSpace;Aim->Node.TranslationMode=BMM_Ignore;Aim->Node.ScaleMode=BMM_Ignore;Aim->AllocateDefaultPins();Link(Previous,Aim->FindPinChecked(TEXT("ComponentPose")));
  auto* Get=Add<UK2Node_VariableGet>(G,600,400);Get->VariableReference.SetSelfMember(TEXT("UpperBodyWorldRotation"));Get->AllocateDefaultPins();Link(Get->GetValuePin(),Aim->FindPinChecked(TEXT("Rotation")));
  auto* ToLocal=Add<UAnimGraphNode_ComponentToLocalSpace>(G,1200,0);ToLocal->AllocateDefaultPins();Link(Aim->FindPinChecked(TEXT("Pose")),ToLocal->FindPinChecked(TEXT("ComponentPose")));Link(ToLocal->FindPinChecked(TEXT("Pose")),In);
 }
 Compile(Anim);Save(Anim);auto* Ch=CastChecked<ACharacter>(BP->GeneratedClass->GetDefaultObject());Ch->GetMesh()->SetAnimInstanceClass(Anim->GeneratedClass);
 auto* HUD=Component<UVTGBossHealthBarComponent>(BP,TEXT("BossHealthBar"),true);HUD->CausticOverlayMaterial=Load<UMaterialInstanceConstant>(TEXT("/InventorySystemX/InventorySystemX/Materials/Material_Instances/MI_Inventory_FogMadeIntoCaustic"));HUD->CausticOverlayOpacity=.4f;
 auto* Behavior=Component<UActorComponent>(BP,TEXT("Boss Behavior"),true);for(const TCHAR* Name:{TEXT("InitialRotationRate"),TEXT("CombatRotationRate"),TEXT("AbilityRotationRate")})CheckedProperty<FNumericProperty>(Behavior->GetClass(),Name)->SetFloatingPointPropertyValue(CheckedProperty<FNumericProperty>(Behavior->GetClass(),Name)->ContainerPtrToValuePtr<void>(Behavior),0);
 Compile(BP);Save(BP);
}
void Acid()
{
 auto* BP=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));Backup(BP);
 auto* Flight=Component<UVTGAcidFlightComponent>(BP,TEXT("AcidFlight"),true);Flight->MaxTravelDistance=1800;
 auto* Move=Component<UProjectileMovementComponent>(BP,TEXT("ProjectileMovement"),true);Move->bRotationFollowsVelocity=true;Move->ProjectileGravityScale=0;Move->InitialSpeed=850;Move->MaxSpeed=850;
 auto* System=Duplicate(Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst")),TEXT("NS_AcidSpit_Projectile"));Backup(System);
 for(auto& Handle:System->GetEmitterHandles())
 {
  auto* Data=Handle.GetEmitterData();check(Data);Data->bLocalSpace=true;
  for(auto* Script:{Data->SpawnScriptProps.Script.Get(),Data->UpdateScriptProps.Script.Get()})if(Script)
  {
   TArray<FNiagaraVariable> Vars;Script->RapidIterationParameters.GetParameters(Vars);
   for(auto& V:Vars)
   {
    const FString Name=V.GetName().ToString();
    if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef())UE_LOG(LogTemp,Display,TEXT("REFINE NIAGARA %s=%f"),*Name,Script->RapidIterationParameters.GetParameterValue<float>(V));
    if(V.GetType()==FNiagaraTypeDefinition::GetVec3Def())UE_LOG(LogTemp,Display,TEXT("REFINE NIAGARA %s=%s"),*Name,*FVector(Script->RapidIterationParameters.GetParameterValue<FVector3f>(V)).ToString());
    // Keep the acid shape local to the damaging projectile; the movement component owns travel.
    if(Name.Contains(TEXT("AddVelocity")) && V.GetType()==FNiagaraTypeDefinition::GetFloatDef() && Name.Contains(TEXT("Speed")))Script->RapidIterationParameters.SetParameterValue<float>(0,V);
    if(Name.Contains(TEXT("Fountain001.RandomRangeFloat.")) && V.GetType()==FNiagaraTypeDefinition::GetFloatDef())Script->RapidIterationParameters.SetParameterValue<float>(0,V);
    if((Name.Contains(TEXT("GravityForce")) || Name.Contains(TEXT("CurlNoiseForce"))) && V.GetType()==FNiagaraTypeDefinition::GetVec3Def())Script->RapidIterationParameters.SetParameterValue<FVector3f>(FVector3f::ZeroVector,V);
    if(Name.Contains(TEXT("CurlNoiseForce")) && V.GetType()==FNiagaraTypeDefinition::GetFloatDef() && Name.Contains(TEXT("Strength")))Script->RapidIterationParameters.SetParameterValue<float>(0,V);
   }
  }
 }
 System->RequestCompile(false);System->WaitForCompilationComplete();Save(System);
 auto* Visual=Component<UNiagaraComponent>(BP,TEXT("AcidVisual"),true);Visual->SetAsset(System);Visual->SetRelativeRotation(FRotator(-90,0,0));Visual->SetRelativeScale3D(FVector(.3));Visual->SetAutoActivate(true);Compile(BP);Save(BP);
 auto* Montage=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_AcidSpit"));Backup(Montage);Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& E){return E.Notify && E.Notify->IsA<UVTGAttackEffectNotify>();});Montage->PostEditChange();Save(Montage);
}
void Dump(UObject* O);
bool Test()
{
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);int Fail=0;
 auto Expect=[&](bool OK,const TCHAR* Label){UE_LOG(LogTemp,Display,TEXT("REFINEMENT %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);Fail+=!OK;};
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SewerBossRefinementTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* BossBP=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster_L3Sewer"));auto* Boss=World->SpawnActor<ACharacter>(BossBP->GeneratedClass,FTransform(FRotator(0,37,0),FVector(3000,0,200)),Spawn);check(Boss);
 auto* Anchor=Boss->FindComponentByClass<UVTGAnchoredBossComponent>();check(Anchor);Anchor->BeginPlay();const FTransform Placed=Boss->GetActorTransform();
 auto* Target=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform(FVector(4000,1500,100)),Spawn);check(Target); // A rootless actor cannot move, add a scene root.
 auto* TargetRoot=NewObject<USceneComponent>(Target);Target->SetRootComponent(TargetRoot);TargetRoot->RegisterComponent();Target->SetActorLocation(FVector(4000,1500,100));
 UActorComponent* Behavior=nullptr;for(auto* C:Boss->GetComponents())if(C->GetClass()->FindPropertyByName(TEXT("AbilityRotationRate")))Behavior=C;check(Behavior);
 Dump(Behavior);auto* PlayerDefaults=Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"))->GeneratedClass->GetDefaultObject();Dump(PlayerDefaults);
 FFileHelper::SaveStringToFile(Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"))->GetTableAsJSON(),*(FPaths::ProjectDir()/TEXT("Scripts/SewerBossRefinement/phases.json")));
 CheckedProperty<FObjectPropertyBase>(Behavior->GetClass(),TEXT("TargetActor"))->SetObjectPropertyValue_InContainer(Behavior,Target);CheckedProperty<FBoolProperty>(Behavior->GetClass(),TEXT("IsAlive"))->SetPropertyValue_InContainer(Behavior,true);
 Boss->SetActorLocationAndRotation(FVector(3333,222,0),FRotator(20,130,40));Anchor->TickComponent(.5f,LEVELTICK_All,nullptr);
 Expect(Boss->GetActorTransform().Equals(Placed,.001),TEXT("L3 actor placement restored despite attempted translation and rotation"));Expect(Boss->GetCharacterMovement()->MovementMode==MOVE_None,TEXT("hanging boss movement disabled"));Expect(!Anchor->UpperBodyWorldRotation.IsNearlyZero(),TEXT("target drives upper spine aim without actor turning"));
 auto* Mesh=Boss->GetMesh();Mesh->InitAnim(true);auto* Anim=Cast<UVTGAnchoredBossAnimInstance>(Mesh->GetAnimInstance());Expect(Anim!=nullptr,TEXT("L3 uses dedicated anchored animation blueprint"));
 if(Anim)
 {
  auto Pose=[&](FRotator Aim){Anchor->UpperBodyWorldRotation=Aim;for(int I=0;I<3;++I){Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();}return Mesh->GetSocketTransform(TEXT("root"),RTS_Component);};
  const auto Root0=Pose(FRotator::ZeroRotator);const auto Spine0=Mesh->GetSocketTransform(TEXT("spine"),RTS_Component);const auto Upper0=Mesh->GetSocketTransform(TEXT("spine_001"),RTS_Component);
  const auto Root1=Pose(FRotator(0,35,0));const auto Spine1=Mesh->GetSocketTransform(TEXT("spine"),RTS_Component);const auto Upper1=Mesh->GetSocketTransform(TEXT("spine_001"),RTS_Component);
  Expect(Root0.Equals(Root1,.001) && Spine0.Equals(Spine1,.001),TEXT("aim leaves evaluated root and lower spine unchanged"));Expect(!Upper0.GetRotation().Equals(Upper1.GetRotation(),.01),TEXT("evaluated spine_001 rotates with aim"));
  auto* Attack=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_AcidSpit"));Anchor->UpperBodyWorldRotation=FRotator::ZeroRotator;Anim->Montage_Play(Attack);const auto Before=Mesh->GetSocketTransform(TEXT("spine_004"),RTS_Component);Mesh->TickAnimation(.45f,false);Mesh->RefreshBoneTransforms();
  Expect(Mesh->GetSocketTransform(TEXT("root"),RTS_Component).Equals(Root0,.01),TEXT("attack montage cannot translate or rotate root"));Expect(!Mesh->GetSocketTransform(TEXT("spine_004"),RTS_Component).Equals(Before,.01),TEXT("authored upper-body attack animation retained"));
 }
 auto* EggBP=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_Egg"));auto* Egg=World->SpawnActor<ACharacter>(EggBP->GeneratedClass,FTransform(FVector(6000,0,500)),Spawn);check(Egg);auto* EggMesh=Egg->GetMesh();EggMesh->InitAnim(true);
 UActorComponent* EggBehavior=nullptr;for(auto* C:Egg->GetComponents())if(C->GetClass()->FindPropertyByName(TEXT("DeathAnim")))EggBehavior=C;check(EggBehavior);
 CheckedProperty<FObjectPropertyBase>(EggBehavior->GetClass(),TEXT("CharacterRef"))->SetObjectPropertyValue_InContainer(EggBehavior,Egg);
 auto* Death=Cast<UAnimMontage>(CheckedProperty<FObjectPropertyBase>(EggBehavior->GetClass(),TEXT("DeathAnim"))->GetObjectPropertyValue_InContainer(EggBehavior));check(Death);
 auto* Seq=Load<UAnimSequence>(TEXT("/Game/Characters/Monster/MonsterEgg/ANM/Monster_Egg1_Death"));Expect(Death->SlotAnimTracks.Num()==1 && Death->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()==Seq,TEXT("egg toolkit DeathAnim plays requested Monster_Egg1_Death sequence"));
 Expect(CheckedProperty<FBoolProperty>(EggBehavior->GetClass(),TEXT("UseDeathAnim"))->GetPropertyValue_InContainer(EggBehavior),TEXT("egg death animation enabled before ragdoll"));
 Expect(EggMesh->GetPhysicsAsset()->GetName()==TEXT("PA_SeaMonsterEgg_ScaledBones"),TEXT("egg uses scale-aware physics asset copy"));
 EggMesh->GetAnimInstance()->Montage_Play(Death);bool Simulated=false;for(int I=0;I<600 && !Simulated;++I){EggMesh->TickAnimation(1.f/60,false);EggMesh->RefreshBoneTransforms();EggMesh->ConditionallyDispatchQueuedAnimEvents();Simulated=EggMesh->IsSimulatingPhysics();}
 Expect(Simulated,TEXT("actual death montage notify hands pose to existing toolkit ragdoll"));
 // Exercise a non-unit, per-bone scale through the same native handoff.
 if(EggMesh->Bodies.Num())
 {
  EggMesh->SetSimulatePhysics(false);EggMesh->SetRelativeScale3D(EggMesh->GetRelativeScale3D()*1.25);EggMesh->RefreshBoneTransforms();const int Index=EggMesh->GetBoneIndex(EggMesh->Bodies[0]->BodySetup.Get()->BoneName);check(Index!=INDEX_NONE);
  auto* Notify=NewObject<UVTGEggRagdollNotify>();Notify->Notify(EggMesh,Death,FAnimNotifyEventReference());FVector Expected=EggMesh->GetBoneTransform(Index).GetScale3D();
  const auto& Geometry=CastChecked<USkeletalBodySetup>(EggMesh->Bodies[0]->BodySetup.Get())->AggGeom;if(Geometry.SphereElems.Num())Expected=FVector(Expected.GetAbsMin());else if(Geometry.SphylElems.Num())Expected.X=Expected.Y=FMath::Max(FMath::Abs(Expected.X),FMath::Abs(Expected.Y));
  UE_LOG(LogTemp,Display,TEXT("REFINEMENT BODY SCALE actual=%s engine-supported expected=%s"),*EggMesh->Bodies[0]->Scale3D.ToString(),*Expected.ToString());Expect(EggMesh->Bodies[0]->Scale3D.Equals(Expected,.01),TEXT("scaled ragdoll body follows pose within Chaos primitive shape limits"));
 }
 auto* ProjectileBP=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));auto* Projectile=World->SpawnActor<AActor>(ProjectileBP->GeneratedClass,FTransform(FRotator(-17,52,0),FVector(0,0,5000)),Spawn);check(Projectile);auto* Flight=Projectile->FindComponentByClass<UVTGAcidFlightComponent>();auto* Move=Projectile->FindComponentByClass<UProjectileMovementComponent>();auto* Visual=Projectile->FindComponentByClass<UNiagaraComponent>();check(Flight && Move && Visual);if(!Move->HasBeenInitialized())Move->InitializeComponent();Move->Activate();Flight->BeginPlay();
 Expect(Move->bRotationFollowsVelocity && Move->ProjectileGravityScale==0,TEXT("projectile faces actual velocity on a straight 3D trajectory"));Expect(Visual->GetAsset()->GetName()==TEXT("NS_AcidSpit_Projectile"),TEXT("acid uses local flight effect instead of independent ballistic burst"));
 for(auto& H:Visual->GetAsset()->GetEmitterHandles())Expect(H.GetEmitterData()->bLocalSpace,TEXT("Niagara emitter travels with projectile collision"));
 const FVector Start=Projectile->GetActorLocation();const FVector Direction=Move->Velocity.GetSafeNormal();const FVector VisualStart=Visual->GetComponentLocation();Move->TickComponent(.1f,LEVELTICK_All,nullptr);Flight->TickComponent(.1f,LEVELTICK_All,nullptr);
 Expect(FVector::DotProduct(Visual->GetUpVector(),Direction)>.999,TEXT("authored Niagara +Z spray axis aligns with projectile +X flight direction"));
 UE_LOG(LogTemp,Display,TEXT("REFINEMENT FLIGHT velocity=%s delta=%s root=%s updated=%s mobility=%d simulate=%d phys=%d skip=%d"),*Move->Velocity.ToString(),*(Projectile->GetActorLocation()-Start).ToString(),*GetNameSafe(Projectile->GetRootComponent()),*GetNameSafe(Move->UpdatedComponent),int(Move->UpdatedComponent->Mobility),Move->bSimulationEnabled,Move->UpdatedComponent->IsSimulatingPhysics(),Move->ShouldSkipUpdate(.1));
 Expect((Projectile->GetActorLocation()-Start).GetSafeNormal().Equals(Direction,.001),TEXT("projectile travel follows launch direction"));Expect((Visual->GetComponentLocation()-VisualStart).Equals(Projectile->GetActorLocation()-Start,.001),TEXT("VFX and damaging collision travel identical distance"));
 Projectile->SetActorLocation(Start+Direction*1790);Flight->TickComponent(0,LEVELTICK_All,nullptr);Expect(!Projectile->IsActorBeingDestroyed(),TEXT("acid remains active before 1800 cm range"));Projectile->SetActorLocation(Start+Direction*1810);Flight->TickComponent(0,LEVELTICK_All,nullptr);Expect(Projectile->IsActorBeingDestroyed(),TEXT("acid VFX and collision expire together at configured range"));
 auto* HUD=Boss->FindComponentByClass<UVTGBossHealthBarComponent>();Expect(HUD && HUD->CausticOverlayMaterial && HUD->CausticOverlayOpacity==.4f,TEXT("L3 boss HUD uses requested caustics material at translucent opacity"));
 auto* Other=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster"));UE_LOG(LogTemp,Display,TEXT("REFINEMENT old boss path resolves to %s"),*Other->GetPathName());Expect(Load<UAnimBlueprint>(Root+TEXT("ABP_EggProducer"))->ParentClass==UAnimInstance::StaticClass(),TEXT("original shared animation blueprint remains unchanged"));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return !Fail;
}
void Dump(UObject* O)
{
 UE_LOG(LogTemp,Display,TEXT("REFINE OBJECT %s"),*O->GetPathName());
 for(TFieldIterator<FProperty> It(O->GetClass());It;++It)
 {
  const auto N=It->GetName();if(!N.Contains(TEXT("Death"))&&!N.Contains(TEXT("Health"))&&!N.Contains(TEXT("Ragdoll"))&&!N.Contains(TEXT("Rotation"))&&!N.Contains(TEXT("Boss"))&&!N.Contains(TEXT("Phase"))&&!N.Contains(TEXT("Target")))continue;
  FString V;It->ExportTextItem_Direct(V,It->ContainerPtrToValuePtr<void>(O),nullptr,O,PPF_None);UE_LOG(LogTemp,Display,TEXT("REFINE PROP %s=%s"),*N,*V.Left(1200));
 }
}
void Audit()
{
 for(const TCHAR* Name:{TEXT("BP_SeaMonster_Egg"),TEXT("BP_Boss_SeaMonster_L3Sewer"),TEXT("BP_SeaMonster_AcidProjectile")})
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,*(Root+Name));check(BP);Dump(BP->GeneratedClass->GetDefaultObject());
  for(UClass* C=BP->GeneratedClass;C;C=C->GetSuperClass())if(auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);B && B->SimpleConstructionScript)
   for(auto* N:B->SimpleConstructionScript->GetAllNodes()){UE_LOG(LogTemp,Display,TEXT("REFINE COMPONENT %s %s"),*N->GetVariableName().ToString(),*N->ComponentTemplate->GetClass()->GetPathName());Dump(N->ComponentTemplate);}
  if(auto* Ch=Cast<ACharacter>(BP->GeneratedClass->GetDefaultObject()))
  {auto* M=Ch->GetMesh();UE_LOG(LogTemp,Display,TEXT("REFINE MESH %s ANIM %s PHYS %s REL %s"),*GetPathNameSafe(M->GetSkeletalMeshAsset()),*GetPathNameSafe(M->GetAnimClass()),*GetPathNameSafe(M->GetPhysicsAsset()),*M->GetRelativeTransform().ToString());if(M->GetSkeletalMeshAsset()){const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();for(int I=0;I<Ref.GetNum();++I)UE_LOG(LogTemp,Display,TEXT("REFINE BONE %d %s %s"),I,*Ref.GetBoneName(I).ToString(),*Ref.GetRefBonePose()[I].ToString());}}
 }
 auto* MI=LoadObject<UMaterialInstanceConstant>(nullptr,TEXT("/InventorySystemX/InventorySystemX/Materials/Material_Instances/MI_Inventory_FogMadeIntoCaustic"));check(MI);UE_LOG(LogTemp,Display,TEXT("REFINE MATERIAL domain=%d"),int(MI->GetMaterial()->MaterialDomain));
}
}
UVTGSewerBossRefinementCommandlet::UVTGSewerBossRefinementCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
extern int32 VTGRenderCombatHUD(const FString& Output);
int32 UVTGSewerBossRefinementCommandlet::Main(const FString& Params){if(Params.Contains(TEXT("Audit"))){SewerRefine::Audit();return 0;}if(Params.Contains(TEXT("Preview")))return VTGRenderCombatHUD(FPaths::ProjectDir()/TEXT("Scripts/SewerBossRefinement/hud-preview.png"));if(!Params.Contains(TEXT("VerifyOnly"))){SewerRefine::Egg();SewerRefine::AnchorBoss();SewerRefine::Acid();}return SewerRefine::Test()?0:1;}



